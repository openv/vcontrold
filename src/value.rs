use core::convert::Infallible;
use std::{fmt, str::FromStr};

use serde::{Deserialize, Serialize};

use crate::{
  conversion::Conversion,
  types::{CircuitTimes, Date, DateTime, DeviceId, DeviceIdF0, Error},
};

#[derive(Debug, Clone)]
pub(crate) struct ConversionError<'c> {
  pub value: Value,
  conversion: &'c Conversion,
}

impl fmt::Display for ConversionError<'_> {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "Conversion {:?} not applicable to value {:?}.", self.conversion, self.value)
  }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Value {
  #[serde(skip_deserializing)]
  DeviceId(DeviceId),
  #[serde(skip_deserializing)]
  DeviceIdF0(DeviceIdF0),
  Int(i64),
  Double(f64),
  ByteArray(Vec<u8>),
  Array(Vec<Value>),
  String(String),
  Date(Date),
  DateTime(DateTime),
  CircuitTimes(Box<CircuitTimes>),
  Error(Error),
  Empty,
}

macro_rules! convert_double {
  ($value:expr, $op:tt, $n:literal) => {
    if let Value::Double(n) = $value {
      return Ok(Value::Double(n $op $n))
    }
  }
}

impl Value {
  pub(crate) fn convert(mut self, conversion: &Conversion) -> Result<Self, ConversionError<'_>> {
    match conversion {
      Conversion::Div2 => convert_double!(self, /, 2.0),
      Conversion::Div5 => convert_double!(self, /, 5.0),
      Conversion::Div10 => convert_double!(self, /, 10.0),
      Conversion::Div100 => convert_double!(self, /, 100.0),
      Conversion::Div1000 => convert_double!(self, /, 1000.0),
      Conversion::Mul2 => convert_double!(self, *, 2.0),
      Conversion::Mul5 => convert_double!(self, *, 5.0),
      Conversion::Mul10 => convert_double!(self, *, 10.0),
      Conversion::Mul100 => convert_double!(self, *, 100.0),
      Conversion::MulOffset { factor, offset } => {
        if let Value::Double(n) = self {
          return Ok(Value::Double(n * factor + offset))
        }
      },
      Conversion::SecToMinute => convert_double!(self, /, 60.0),
      Conversion::SecToHour => convert_double!(self, /, 3600.0),
      Conversion::HexByteToAsciiByte => {
        if let Value::ByteArray(bytes) = self {
          let s = bytes.iter().filter(|b| **b != b'0').map(|b| char::from(*b)).collect::<String>();
          return Ok(Value::String(s))
        }
      },
      Conversion::HexByteToVersion => {
        if let Value::ByteArray(bytes) = self {
          return Ok(Value::String(bytes.iter().map(|b| b.to_string()).collect::<Vec<_>>().join(".")))
        }
      },
      Conversion::RotateBytes => match self {
        Value::ByteArray(ref mut array) => {
          array.reverse();
          return Ok(self)
        },
        Value::Int(n) => return Ok(Value::Int(n)),
        _ => (),
      },
      _ => (),
    }

    Err(ConversionError { value: self, conversion })
  }

  pub(crate) fn convert_back(self, conversion: &Conversion) -> Result<Self, ConversionError<'_>> {
    match conversion {
      Conversion::Div2 => convert_double!(self, *, 2.0),
      Conversion::Div5 => convert_double!(self, *, 5.0),
      Conversion::Div10 => convert_double!(self, *, 10.0),
      Conversion::Div100 => convert_double!(self, *, 100.0),
      Conversion::Div1000 => convert_double!(self, *, 1000.0),
      Conversion::Mul2 => convert_double!(self, /, 2.0),
      Conversion::Mul5 => convert_double!(self, /, 5.0),
      Conversion::Mul10 => convert_double!(self, /, 10.0),
      Conversion::Mul100 => convert_double!(self, /, 100.0),
      Conversion::MulOffset { factor, offset } => {
        if let Value::Double(n) = self {
          return Ok(Value::Double((n - offset) / factor))
        }
      },
      _ => (),
    }

    Err(ConversionError { value: self, conversion })
  }
}

#[derive(Debug, Serialize)]
pub struct OutputValue {
  pub value: Value,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub unit: Option<&'static str>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub mapping: Option<&'static phf::map::Map<i32, &'static str>>,
}

impl fmt::Display for OutputValue {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    match &self.value {
      Value::DeviceId(device_id) => write!(f, "{:#?}", device_id)?,
      Value::DeviceIdF0(device_id_f0) => write!(f, "{:#?}", device_id_f0)?,
      Value::Int(n) => {
        if let Some(mapping) = self.mapping {
          write!(f, "{}", mapping.get(&(*n as i32)).unwrap())?;
        } else {
          write!(f, "{}", n)?;
        }
      },
      Value::Double(n) => write!(f, "{}", n)?,
      Value::Array(array) => write!(f, "{:?}", array)?,
      Value::ByteArray(byte_array) => write!(f, "{:?}", byte_array)?,
      Value::Date(date) => write!(f, "{}", date)?,
      Value::DateTime(date_time) => write!(f, "{}", date_time)?,
      Value::Error(error) => {
        write!(f, "{}", self.mapping.unwrap().get(&(error.index() as i32)).unwrap())?;
      },
      Value::CircuitTimes(cycle_times) => write!(f, "{:#?}", cycle_times)?,
      Value::String(string) => write!(f, "{}", string)?,
      Value::Empty => return Ok(()),
    }

    if let Some(unit) = self.unit {
      write!(f, " {}", unit)?;
    }

    Ok(())
  }
}

impl FromStr for Value {
  type Err = Infallible;

  fn from_str(s: &str) -> Result<Value, Self::Err> {
    if let Ok(number) = s.parse::<i64>() {
      return Ok(Value::Int(number))
    }

    if let Ok(number) = s.parse::<f64>() {
      return Ok(Value::Double(number))
    }

    // if let Ok(cycletime) = s.parse::<[CircuitTime; 4]>() {
    //   return Ok(Value::CircuitTimes(cycletime))
    // }

    Ok(Value::String(s.to_owned()))
  }
}
