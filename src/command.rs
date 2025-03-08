use arrayref::array_ref;

use crate::{
  conversion::Conversion,
  protocol::Protocol,
  types::{self, CircuitTimes, Date, DateTime, DeviceId, DeviceIdF0},
  AccessMode, DataType, Error, Optolink, Parameter, Value,
};

/// A command which can be executed on an Optolink connection.
#[derive(Debug)]
pub struct Command {
  pub(crate) addr: u16,
  pub(crate) mode: AccessMode,
  pub(crate) data_type: DataType,
  pub(crate) parameter: Parameter,
  pub(crate) block_count: Option<usize>,
  pub(crate) block_len: usize,
  pub(crate) byte_len: usize,
  pub(crate) byte_pos: usize,
  pub(crate) bit_pos: usize,
  pub(crate) bit_len: Option<usize>,
  pub(crate) conversion: Option<Conversion>,
  pub(crate) lower_bound: Option<f64>,
  pub(crate) upper_bound: Option<f64>,
  pub(crate) unit: Option<&'static str>,
  pub(crate) mapping: Option<phf::map::Map<i32, &'static str>>,
}

impl Command {
  /// Get the command's access mode.
  pub fn access_mode(&self) -> AccessMode {
    self.mode
  }

  pub(crate) fn parse_value(&self, bytes: &[u8]) -> Result<Value, Error> {
    if bytes.iter().all(|&b| b == 0xff) {
      return Ok(Value::Empty)
    }

    let mut value = match &self.data_type {
      DataType::DeviceId => {
        if bytes.len() != 8 {
          return Err(Error::InvalidFormat("array length is not 8".to_string()))
        }

        Value::DeviceId(DeviceId::from_bytes(array_ref![bytes, 0, 8]))
      },
      DataType::DeviceIdF0 => {
        if bytes.len() != 2 {
          return Err(Error::InvalidFormat("array length is not 2".to_string()))
        }

        Value::DeviceIdF0(DeviceIdF0::from_bytes(array_ref![bytes, 0, 2]))
      },
      DataType::Date => {
        if bytes.len() != 8 {
          return Err(Error::InvalidFormat("array length is not 8".to_string()))
        }

        Value::Date(Date::from_bytes(array_ref![bytes, 0, 8])?)
      },
      DataType::DateTime => {
        if bytes.len() != 8 {
          return Err(Error::InvalidFormat("array length is not 8".to_string()))
        }

        Value::DateTime(DateTime::from_bytes(array_ref![bytes, 0, 8])?)
      },
      DataType::CircuitTimes => {
        if bytes.len() != 56 {
          return Err(Error::InvalidFormat("array length is not 56".to_string()))
        }

        Value::CircuitTimes(Box::new(CircuitTimes::from_bytes(array_ref![bytes, 0, 56])))
      },
      DataType::ErrorIndex => {
        if bytes.len() != 10 {
          return Err(Error::InvalidFormat("array length is not 10".to_string()))
        }

        let errors = bytes.iter().copied().take_while(|&b| b != 0).collect();
        Value::ByteArray(errors)
      },
      DataType::Error => {
        if bytes.len() != 9 {
          return Err(Error::InvalidFormat("array length is not 9".to_string()))
        }

        Value::Error(types::Error::from_bytes(array_ref![bytes, 0, 9])?)
      },
      DataType::String => {
        let end = bytes.iter().position(|&c| c == b'\0').unwrap_or(bytes.len());

        match String::from_utf8(bytes[..end].to_vec()) {
          Ok(s) => Value::String(s),
          Err(err) => return Err(Error::Utf8(err)),
        }
      },
      DataType::ByteArray => Value::ByteArray(bytes.to_vec()),
      t => {
        let mut n: i32 = 0;

        if let Some(bit_len) = self.bit_len {
          for i in 0..bit_len {
            let bit_pos = self.bit_pos + i;

            let byte = bit_pos / 8;
            let bit = bit_pos % 8;
            let bit_mask = 0b10000000 >> bit;

            n <<= 1;

            if (bytes[byte] & bit_mask) != 0 {
              n |= 0b1;
            }
          }
        } else {
          #[allow(arithmetic_overflow)]
          match self.parameter {
            Parameter::IntHighByteFirst
            | Parameter::Int4HighByteFirst
            | Parameter::SIntHighByteFirst
            | Parameter::SInt4HighByteFirst => {
              for &b in bytes.iter().take(4) {
                n = (n << 8) | (b as i32);
              }
            },
            _ => {
              for &b in bytes.iter().rev().take(4) {
                n = (n << 8) | (b as i32);
              }
            },
          }
        }

        match t {
          data_type @ DataType::Byte => match &self.parameter {
            Parameter::SByte => Value::Int(n as u8 as i64),
            Parameter::Byte => Value::Int(n as u8 as i64),
            parameter => unreachable!("Data type {:?} with parameter {:?}.", data_type, parameter),
          },
          data_type @ DataType::Int => match &self.parameter {
            Parameter::Byte => Value::Int(n as i64),
            Parameter::Int | Parameter::IntHighByteFirst => Value::Int(n as i64),
            Parameter::Int4 | Parameter::Int4HighByteFirst => Value::Int(n as i64),
            Parameter::SByte => Value::Int(n as i64),
            Parameter::SInt | Parameter::SIntHighByteFirst => Value::Int(n as i64),
            Parameter::SInt4 | Parameter::SInt4HighByteFirst => Value::Int(n as i64),
            parameter => unreachable!("Data type {:?} with parameter {:?} ({:?}).", data_type, parameter, bytes),
          },
          data_type @ DataType::Double => match &self.parameter {
            Parameter::Byte => Value::Double(n as f64),
            Parameter::Int | Parameter::IntHighByteFirst => Value::Double(n as f64),
            Parameter::Int4 | Parameter::Int4HighByteFirst => Value::Double(n as f64),
            Parameter::SByte => Value::Double(n as f64 as i8 as f64),
            Parameter::SInt | Parameter::SIntHighByteFirst => Value::Double(n as f64 as i16 as f64),
            Parameter::SInt4 | Parameter::SInt4HighByteFirst => Value::Double(n as f64 as i32 as f64),
            parameter => unreachable!("Data type {:?} with parameter {:?}.", data_type, parameter),
          },
          _ => unreachable!(),
        }
      },
    };

    if let Some(conversion) = &self.conversion {
      value = match value.convert(conversion) {
        Ok(value) => value,
        Err(err) => {
          log::warn!("Failed to convert 0x{:04X}: {err}", self.addr);
          err.value
        },
      };
    }

    Ok(value)
  }

  pub async fn get(&self, o: &mut Optolink, protocol: Protocol) -> Result<Value, Error> {
    log::trace!("Command::get(…)");

    if !self.mode.is_read() {
      return Err(Error::UnsupportedMode(format!("Address 0x{:04X} does not support reading.", self.addr)))
    }

    let mut buf = vec![0; self.block_len];
    protocol.get(o, self.addr, &mut buf).await?;

    let bytes = &buf[self.byte_pos..(self.byte_pos + self.byte_len)];

    if let Some(block_count) = self.block_count {
      let block_len = self.block_len / block_count;

      let mut values = vec![];
      for i in 0..block_count {
        let start = i * block_len;
        let value = self.parse_value(&bytes[(start)..(start + block_len)])?;
        values.push(value);
      }

      Ok(Value::Array(values))
    } else {
      self.parse_value(bytes)
    }
  }

  pub async fn set(&self, o: &mut Optolink, protocol: Protocol, mut input: Value) -> Result<(), Error> {
    log::trace!("Command::set(…)");

    if !self.mode.is_write() {
      return Err(Error::UnsupportedMode(format!("Address 0x{:04X} does not support writing.", self.addr)))
    }

    if let Some(conversion) = &self.conversion {
      input = input.convert_back(conversion).unwrap();
    }

    let bytes = match (&self.data_type, input) {
      (DataType::DateTime, Value::DateTime(date_time)) => date_time.to_bytes().to_vec(),
      (DataType::CircuitTimes, Value::CircuitTimes(cycletimes)) => cycletimes.to_bytes().to_vec(),
      (DataType::ByteArray, Value::ByteArray(bytes)) => bytes.to_vec(),
      (DataType::String, Value::String(s)) => s.as_bytes().to_vec(),
      (DataType::Error, Value::Error(error)) => error.to_bytes().to_vec(),
      (DataType::Int | DataType::Byte, Value::Int(n)) => {
        if let Some(lower_bound) = self.lower_bound {
          let lower_bound = lower_bound as _;
          if n < lower_bound {
            return Err(Error::InvalidArgument(format!("{} is less than minimum {}", n, lower_bound)))
          }
        }

        if let Some(upper_bound) = self.upper_bound {
          let upper_bound = upper_bound as _;
          if n > upper_bound {
            return Err(Error::InvalidArgument(format!("{} is greater than maximum {}", n, upper_bound)))
          }
        }

        match self.parameter {
          Parameter::Byte => (n as u8).to_le_bytes().to_vec(),
          Parameter::Int => (n as u16).to_le_bytes().to_vec(),
          Parameter::IntHighByteFirst => (n as u16).to_be_bytes().to_vec(),
          Parameter::Int4 => (n as u32).to_le_bytes().to_vec(),
          Parameter::Int4HighByteFirst => (n as u32).to_be_bytes().to_vec(),
          Parameter::SByte => (n as i8).to_le_bytes().to_vec(),
          Parameter::SInt => (n as i16).to_le_bytes().to_vec(),
          Parameter::SIntHighByteFirst => (n as i16).to_be_bytes().to_vec(),
          Parameter::SInt4 => (n as i32).to_le_bytes().to_vec(),
          Parameter::SInt4HighByteFirst => (n as i32).to_be_bytes().to_vec(),
          _ => unreachable!(),
        }
      },
      (DataType::Double, Value::Double(n)) => {
        if let Some(lower_bound) = self.lower_bound {
          if n < lower_bound {
            return Err(Error::InvalidArgument(format!("{} is less than minimum {}", n, lower_bound)))
          }
        }

        if let Some(upper_bound) = self.upper_bound {
          if n > upper_bound {
            return Err(Error::InvalidArgument(format!("{} is greater than maximum {}", n, upper_bound)))
          }
        }

        match self.parameter {
          Parameter::Byte => (n as u8).to_le_bytes().to_vec(),
          Parameter::Int => (n as u16).to_le_bytes().to_vec(),
          Parameter::IntHighByteFirst => (n as u16).to_be_bytes().to_vec(),
          Parameter::Int4 => (n as u32).to_le_bytes().to_vec(),
          Parameter::Int4HighByteFirst => (n as u32).to_be_bytes().to_vec(),
          Parameter::SByte => (n as i8).to_le_bytes().to_vec(),
          Parameter::SInt => (n as i16).to_le_bytes().to_vec(),
          Parameter::SIntHighByteFirst => (n as i16).to_be_bytes().to_vec(),
          Parameter::SInt4 => (n as i32).to_le_bytes().to_vec(),
          Parameter::SInt4HighByteFirst => (n as i32).to_be_bytes().to_vec(),
          _ => unreachable!(),
        }
      },
      (data_type, input) => return Err(Error::InvalidArgument(format!("expected {:?}, got {:?}", data_type, input))),
    };

    protocol.set(o, self.addr, &bytes).await.map_err(Into::into)
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn parse_double_from_int() {
    let command = Command {
      addr: 0x0886,
      mode: AccessMode::Read,
      data_type: DataType::Double,
      parameter: Parameter::Int,
      block_count: None,
      block_len: 4,
      byte_len: 4,
      byte_pos: 0,
      bit_len: None,
      bit_pos: 0,
      conversion: Some(Conversion::SecToHour),
      lower_bound: None,
      upper_bound: None,
      unit: Some("h"),
      mapping: None,
    };
    let value = command.parse_value(&[0x00, 0x95, 0xBA, 0x0A]).unwrap();
    assert_eq!(value, Value::Double(50000.0));
  }

  // `Ecotronic_0D00` specifies `RotateBytes` for `Int`, which doesn't seem to do anything.
  #[test]
  fn parse_rotate_bytes_int() {
    let command = Command {
      addr: 0x0D00,
      mode: AccessMode::Read,
      data_type: DataType::Int,
      parameter: Parameter::Int,
      block_count: None,
      block_len: 2,
      byte_len: 2,
      byte_pos: 0,
      bit_len: None,
      bit_pos: 0,
      conversion: Some(Conversion::RotateBytes),
      lower_bound: None,
      upper_bound: None,
      unit: None,
      mapping: None,
    };

    let value = command.parse_value(&[0xb3, 0x04]).unwrap();
    assert_eq!(value, Value::Int(1203));
  }
}
