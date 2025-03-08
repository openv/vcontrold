use core::{fmt, str::FromStr};

use arrayref::array_ref;
#[cfg(feature = "impl_json_schema")]
use schemars::JsonSchema;
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};

#[cfg_attr(feature = "impl_json_schema", derive(JsonSchema))]
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CircuitTimes {
  mon: CircuitTime,
  tue: CircuitTime,
  wed: CircuitTime,
  thu: CircuitTime,
  fri: CircuitTime,
  sat: CircuitTime,
  sun: CircuitTime,
}

impl CircuitTimes {
  #[rustfmt::skip]
  pub fn from_bytes(bytes: &[u8; 56]) -> Self {
    Self {
      mon: CircuitTime::from_bytes(array_ref![bytes,  0, 8]),
      tue: CircuitTime::from_bytes(array_ref![bytes,  8, 8]),
      wed: CircuitTime::from_bytes(array_ref![bytes, 16, 8]),
      thu: CircuitTime::from_bytes(array_ref![bytes, 24, 8]),
      fri: CircuitTime::from_bytes(array_ref![bytes, 32, 8]),
      sat: CircuitTime::from_bytes(array_ref![bytes, 40, 8]),
      sun: CircuitTime::from_bytes(array_ref![bytes, 48, 8]),
    }
  }

  #[rustfmt::skip]
  pub fn to_bytes(&self) -> [u8; 56] {
    let mon = self.mon.to_bytes();
    let tue = self.tue.to_bytes();
    let wed = self.wed.to_bytes();
    let thu = self.thu.to_bytes();
    let fri = self.fri.to_bytes();
    let sat = self.sat.to_bytes();
    let sun = self.sun.to_bytes();

    [
      mon[0], mon[1], mon[2], mon[3], mon[4], mon[5], mon[6], mon[7],
      tue[0], tue[1], tue[2], tue[3], tue[4], tue[5], tue[6], tue[7],
      wed[0], wed[1], wed[2], wed[3], wed[4], wed[5], wed[6], wed[7],
      thu[0], thu[1], thu[2], thu[3], thu[4], thu[5], thu[6], thu[7],
      fri[0], fri[1], fri[2], fri[3], fri[4], fri[5], fri[6], fri[7],
      sat[0], sat[1], sat[2], sat[3], sat[4], sat[5], sat[6], sat[7],
      sun[0], sun[1], sun[2], sun[3], sun[4], sun[5], sun[6], sun[7],
    ]
  }
}

#[cfg_attr(feature = "impl_json_schema", derive(JsonSchema))]
#[derive(Clone, PartialEq, Serialize, Deserialize)]
pub struct CircuitTime([Option<TimeSpan>; 4]);

impl CircuitTime {
  pub fn from_bytes(bytes: &[u8; 8]) -> Self {
    Self([
      Time::from_byte(bytes[0]).zip(Time::from_byte(bytes[1])).map(|(from, to)| TimeSpan { from, to }),
      Time::from_byte(bytes[2]).zip(Time::from_byte(bytes[3])).map(|(from, to)| TimeSpan { from, to }),
      Time::from_byte(bytes[4]).zip(Time::from_byte(bytes[5])).map(|(from, to)| TimeSpan { from, to }),
      Time::from_byte(bytes[6]).zip(Time::from_byte(bytes[7])).map(|(from, to)| TimeSpan { from, to }),
    ])
  }

  #[rustfmt::skip]
  pub fn to_bytes(&self) -> [u8; 8] {
    let timespan1 = self.0[0].map(|t| t.to_bytes()).unwrap_or([0xff, 0xff]);
    let timespan2 = self.0[1].map(|t| t.to_bytes()).unwrap_or([0xff, 0xff]);
    let timespan3 = self.0[2].map(|t| t.to_bytes()).unwrap_or([0xff, 0xff]);
    let timespan4 = self.0[3].map(|t| t.to_bytes()).unwrap_or([0xff, 0xff]);

    [
      timespan1[0], timespan1[1],
      timespan2[0], timespan2[1],
      timespan3[0], timespan3[1],
      timespan4[0], timespan4[1],
    ]
  }
}

impl fmt::Display for CircuitTime {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    let mut comma = false;
    for timespan in self.0 {
      if comma {
        write!(f, ", ")?;
      }

      if let Some(timespan) = timespan {
        write!(f, "{}", timespan)?;
      } else {
        write!(f, "--:-- – --:--")?;
      }

      comma = true;
    }

    Ok(())
  }
}

impl fmt::Debug for CircuitTime {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "CircuitTime(")?;
    fmt::Display::fmt(self, f)?;
    write!(f, ")")
  }
}

#[cfg_attr(feature = "impl_json_schema", derive(JsonSchema))]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
struct TimeSpan {
  from: Time,
  to: Time,
}

impl TimeSpan {
  pub fn to_bytes(self) -> [u8; 2] {
    [self.from.to_byte(), self.to.to_byte()]
  }
}

impl fmt::Display for TimeSpan {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{} – {}", self.from, self.to)
  }
}

#[cfg_attr(feature = "impl_json_schema", derive(JsonSchema))]
#[derive(Debug, Clone, Copy, PartialEq)]
struct Time {
  hour: u8,
  minute: u8,
}

impl Time {
  pub const fn from_byte(byte: u8) -> Option<Self> {
    match byte {
      0xff => None,
      byte => {
        let hour = byte >> 3;
        let minute = (byte & 0b111) * 10;

        assert!(hour <= 24);
        assert!(minute < 60);

        Some(Self { hour, minute })
      },
    }
  }

  pub const fn to_byte(self) -> u8 {
    (self.hour << 3) | (self.minute / 10)
  }
}

impl fmt::Display for Time {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{:02}:{:02}", self.hour, self.minute)
  }
}

impl FromStr for Time {
  type Err = &'static str;

  fn from_str(s: &str) -> Result<Self, Self::Err> {
    let mut chars = s.chars();

    fn char_to_u8(c: char) -> Option<u8> {
      if c.is_ascii_digit() {
        return Some(c as u8 - b'0')
      }

      None
    }

    let h1 = chars.next().and_then(char_to_u8).ok_or("first hour character is not a number")?;
    let h2 = chars.next().and_then(char_to_u8).ok_or("second hour character is not a number")?;
    chars.next().filter(|&sep| sep == ':').ok_or("separator is not ':'")?;
    let m1 = chars.next().and_then(char_to_u8).ok_or("first minute character is not a number")?;
    let m2 = chars.next().and_then(char_to_u8).ok_or("second minute character is not a number")?;

    let hour = h1 * 10 + h2;

    if hour > 24 {
      return Err("hour out of range")
    }

    let minute = m1 * 10 + m2;

    if minute >= 60 {
      return Err("minute out of range")
    }

    Ok(Time { hour, minute })
  }
}

impl<'de> Deserialize<'de> for Time {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let s = String::deserialize(deserializer)?;
    s.parse::<Time>().map_err(de::Error::custom)
  }
}

impl Serialize for Time {
  fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
    serializer.serialize_str(&self.to_string())
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn time_from_str_24() {
    let s = r#""24:00""#;

    let time = serde_json::from_str::<Time>(s).unwrap();
    assert_eq!(time, Time { hour: 24, minute: 0 });
  }
}
