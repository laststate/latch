#![no_std]

use core::ffi::{CStr, c_char, c_void};

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ResultCode {
    Ok = 0,
    Invalid = -1,
    NoSpace = -2,
    Io = -3,
    Corrupt = -4,
    Again = -5,
    NotSupported = -6,
    Busy = -7,
    Auth = -8,
    Overflow = -9,
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Severity {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4,
}

#[repr(C)]
pub struct Identity {
    pub project_id: *const c_char,
    pub device_id: *const c_char,
    pub product: *const c_char,
    pub hardware_revision: *const c_char,
    pub bom_revision: *const c_char,
    pub manufacturing_batch: *const c_char,
    pub firmware_version: *const c_char,
    pub firmware_build_id: *const c_char,
    pub bootloader_version: *const c_char,
    pub git_commit: *const c_char,
    pub variant: *const c_char,
    pub architecture: *const c_char,
    pub rtos: *const c_char,
    pub region: *const c_char,
    pub device_group: *const c_char,
}

unsafe extern "C" {
    fn ls_boot() -> ResultCode;
    fn ls_flush() -> ResultCode;
    fn ls_breadcrumb(message: *const c_char);
    fn ls_metric_i32(name: *const c_char, value: i32);
    fn ls_metric_u32(name: *const c_char, value: u32);
    fn ls_capture_message(message: *const c_char, severity: Severity);
    fn ls_span_begin(id: u16) -> ResultCode;
    fn ls_span_end(id: u16) -> ResultCode;
    fn ls_boot_loop_detected() -> bool;
    fn ls_build_id() -> *const c_char;
}

pub fn boot() -> ResultCode {
    unsafe { ls_boot() }
}
pub fn flush() -> ResultCode {
    unsafe { ls_flush() }
}
pub fn breadcrumb(message: &CStr) {
    unsafe { ls_breadcrumb(message.as_ptr()) }
}
pub fn metric_i32(name: &CStr, value: i32) {
    unsafe { ls_metric_i32(name.as_ptr(), value) }
}
pub fn metric_u32(name: &CStr, value: u32) {
    unsafe { ls_metric_u32(name.as_ptr(), value) }
}
pub fn capture(message: &CStr, severity: Severity) {
    unsafe { ls_capture_message(message.as_ptr(), severity) }
}
pub fn boot_loop_detected() -> bool {
    unsafe { ls_boot_loop_detected() }
}
pub fn build_id() -> &'static CStr {
    unsafe { CStr::from_ptr(ls_build_id()) }
}

pub struct Span {
    id: u16,
    active: bool,
}
impl Span {
    pub fn begin(id: u16) -> Self {
        let active = unsafe { ls_span_begin(id) == ResultCode::Ok };
        Self { id, active }
    }
    pub fn finish(mut self) -> ResultCode {
        self.active = false;
        unsafe { ls_span_end(self.id) }
    }
}
impl Drop for Span {
    fn drop(&mut self) {
        if self.active {
            let _ = unsafe { ls_span_end(self.id) };
        }
    }
}

pub type OpaqueContext = *mut c_void;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DecodeError {
    TooShort,
    Magic,
    Version,
    Length,
    HeaderCrc,
    PayloadCrc,
    Tlv,
}

pub struct Envelope<'a> {
    pub version: u8,
    pub message_type: u8,
    pub architecture: u8,
    pub flags: u8,
    pub sequence: u32,
    pub event_id: u32,
    payload: &'a [u8],
}
impl<'a> Envelope<'a> {
    pub fn parse(data: &'a [u8]) -> Result<Self, DecodeError> {
        if data.len() < 28 {
            return Err(DecodeError::TooShort);
        }
        if read_u32(data) != 0x5054_534c {
            return Err(DecodeError::Magic);
        }
        if data[4] != 1 {
            return Err(DecodeError::Version);
        }
        let payload_len = read_u32(&data[16..]) as usize;
        let authentication = if data[7] & 1 != 0 { 32 } else { 0 };
        if data.len() != 24 + payload_len + 4 + authentication {
            return Err(DecodeError::Length);
        }
        if read_u32(&data[20..]) != crc32(&data[..20]) {
            return Err(DecodeError::HeaderCrc);
        }
        let payload = &data[24..24 + payload_len];
        if read_u32(&data[24 + payload_len..]) != crc32(payload) {
            return Err(DecodeError::PayloadCrc);
        }
        Ok(Self {
            version: data[4],
            message_type: data[5],
            architecture: data[6],
            flags: data[7],
            sequence: read_u32(&data[8..]),
            event_id: read_u32(&data[12..]),
            payload,
        })
    }
    pub fn tlvs(&self) -> TlvIterator<'a> {
        TlvIterator {
            remaining: self.payload,
            failed: false,
        }
    }
}

pub struct Tlv<'a> {
    pub field_type: u16,
    pub value: &'a [u8],
}
pub struct TlvIterator<'a> {
    remaining: &'a [u8],
    failed: bool,
}
impl<'a> Iterator for TlvIterator<'a> {
    type Item = Result<Tlv<'a>, DecodeError>;
    fn next(&mut self) -> Option<Self::Item> {
        if self.remaining.is_empty() || self.failed {
            return None;
        }
        if self.remaining.len() < 4 {
            self.failed = true;
            return Some(Err(DecodeError::Tlv));
        }
        let field_type = read_u16(self.remaining);
        let length = read_u16(&self.remaining[2..]) as usize;
        if self.remaining.len() < 4 + length {
            self.failed = true;
            return Some(Err(DecodeError::Tlv));
        }
        let value = &self.remaining[4..4 + length];
        self.remaining = &self.remaining[4 + length..];
        Some(Ok(Tlv { field_type, value }))
    }
}

fn read_u16(data: &[u8]) -> u16 {
    u16::from_le_bytes([data[0], data[1]])
}
fn read_u32(data: &[u8]) -> u32 {
    u32::from_le_bytes([data[0], data[1], data[2], data[3]])
}
pub fn crc32(data: &[u8]) -> u32 {
    let mut crc = 0xffff_ffffu32;
    for byte in data {
        crc ^= *byte as u32;
        for _ in 0..8 {
            crc = (crc >> 1) ^ (0xedb8_8320u32 & (0u32.wrapping_sub(crc & 1)));
        }
    }
    !crc
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn crc_vector() {
        assert_eq!(crc32(b"123456789"), 0xcbf4_3926);
    }
    #[test]
    fn parses_tlv() {
        let payload = [1u8, 0, 2, 0, 0xaa, 0xbb];
        let mut data = [0u8; 34];
        data[0..4].copy_from_slice(&0x5054_534cu32.to_le_bytes());
        data[4] = 1;
        data[5] = 2;
        data[8..12].copy_from_slice(&7u32.to_le_bytes());
        data[12..16].copy_from_slice(&9u32.to_le_bytes());
        data[16..20].copy_from_slice(&(payload.len() as u32).to_le_bytes());
        let header_crc = crc32(&data[..20]);
        data[20..24].copy_from_slice(&header_crc.to_le_bytes());
        data[24..30].copy_from_slice(&payload);
        data[30..34].copy_from_slice(&crc32(&payload).to_le_bytes());
        let envelope = Envelope::parse(&data).unwrap();
        let tlv = envelope.tlvs().next().unwrap().unwrap();
        assert_eq!(tlv.field_type, 1);
        assert_eq!(tlv.value, &[0xaa, 0xbb]);
    }
}
