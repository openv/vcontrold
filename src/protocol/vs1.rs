use std::{
  io,
  time::{Duration, Instant},
};

use tokio::io::{AsyncReadExt, AsyncWriteExt};

use crate::Optolink;

const RESET: u8 = 0x04;
const SYNC: u8 = 0x05;

#[allow(unused)]
#[non_exhaustive]
#[repr(u8)]
enum Function {
  VirtualRead  = 247,
  VirtualWrite = 244,
  GfaRead      = 107,
  GfaWrite     = 104,
  ProcessRead  = 123,
  ProcessWrite = 120,
}

#[derive(Debug)]
pub enum Vs1 {}

impl Vs1 {
  async fn sync(o: &mut Optolink) -> Result<(), std::io::Error> {
    log::trace!("Vs1::sync(…)");

    let mut buf = [0xff];

    // Reset the Optolink connection to get a faster `SYNC` (`0x05`).
    Self::negotiate(o).await?;

    loop {
      log::trace!("Vs1::sync(…) loop");

      if o.read_exact(&mut buf).await.is_ok() && buf == [SYNC] {
        o.purge().await?;
        return Ok(())
      }
    }
  }

  pub async fn negotiate(o: &mut Optolink) -> Result<(), io::Error> {
    log::trace!("Vs1::negotiate(…)");

    o.purge().await?;
    o.write_all(&[RESET]).await?;
    o.flush().await?;

    Ok(())
  }

  pub async fn get(o: &mut Optolink, addr: u16, buf: &mut [u8]) -> Result<(), io::Error> {
    log::trace!("Vs1::get(…)");

    let mut vec = Vec::new();
    vec.extend(&[0x01, Function::VirtualRead as u8]);
    vec.extend(addr.to_be_bytes());
    vec.extend(&[buf.len() as u8]);

    Self::sync(o).await?;

    loop {
      log::trace!("Vs1::get(…) loop");

      o.write_all(&vec).await?;
      o.flush().await?;

      let read_start = Instant::now();

      o.read_exact(buf).await?;

      let stop = Instant::now();

      // Retry if the response contains `SYNC` (`0x05`),
      // since these could be synchronization bytes.
      if buf.contains(&SYNC) {
        let read_time = stop - read_start;

        log::debug!(
          "Vs1::get(…) buf = {}",
          buf.iter().map(|b| format!("{:02X}", b)).collect::<Vec<String>>().join(" ")
        );
        log::debug!("Vs1::get(…) read_time = {:?}", read_time);

        // Return `Ok` if the response was received in a short amount of time,
        // since then they most likely are not synchronization bytes.
        if read_time < Duration::from_millis(500 * buf.len() as u64) {
          return Ok(())
        }

        o.purge().await?;
      } else {
        return Ok(())
      }
    }
  }

  pub async fn set(o: &mut Optolink, addr: u16, value: &[u8]) -> Result<(), io::Error> {
    log::trace!("Vs1::set(…)");

    let mut vec = Vec::new();
    vec.extend(&[0x01, Function::VirtualWrite as u8]);
    vec.extend(addr.to_be_bytes());
    vec.extend(&[value.len() as u8]);
    vec.extend(value);

    Self::sync(o).await?;

    loop {
      o.write_all(&vec).await?;
      o.flush().await?;

      let mut buf = [0xff];
      o.read_exact(&mut buf).await?;

      if buf == [0x00] {
        return Ok(())
      }
    }
  }
}
