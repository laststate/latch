#![no_std]

use core::ffi::{CStr, c_char, c_void};

pub const LEP_HEADER_SIZE: usize = 24;
pub const LEP_VERSION_1: u8 = 1;
pub const LEP_CURRENT_VERSION: u8 = LEP_VERSION_1;
pub const ENVELOPE_AUTHENTICATED: u8 = 1;
pub const ENVELOPE_ENCRYPTED: u8 = 2;
pub const ENVELOPE_AEAD: u8 = 4;
pub const ENVELOPE_TRUNCATED: u8 = 8;
pub const ENVELOPE_KNOWN_FLAGS: u8 =
    ENVELOPE_AUTHENTICATED | ENVELOPE_ENCRYPTED | ENVELOPE_AEAD | ENVELOPE_TRUNCATED;
pub const ENVELOPE_SECURITY_METADATA_SIZE: usize = 28;
pub const AEAD_TAG_SIZE: usize = 16;
pub const HMAC_SHA256_SIZE: usize = 32;

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
    Flags,
    Length,
    HeaderCrc,
    PayloadCrc,
    Payload,
    Encrypted,
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
    encrypted: bool,
}
impl<'a> Envelope<'a> {
    pub fn parse(data: &'a [u8]) -> Result<Self, DecodeError> {
        if data.len() < LEP_HEADER_SIZE + 4 {
            return Err(DecodeError::TooShort);
        }
        if read_u32(data) != 0x5054_534c {
            return Err(DecodeError::Magic);
        }
        if data[4] != LEP_CURRENT_VERSION {
            return Err(DecodeError::Version);
        }

        let flags = data[7];
        if !flags_valid(flags) {
            return Err(DecodeError::Flags);
        }
        let encrypted = flags & ENVELOPE_ENCRYPTED != 0;
        let metadata_size = if encrypted {
            ENVELOPE_SECURITY_METADATA_SIZE
        } else {
            0
        };
        let authentication_size = if encrypted {
            AEAD_TAG_SIZE
        } else if flags & ENVELOPE_AUTHENTICATED != 0 {
            HMAC_SHA256_SIZE
        } else {
            0
        };
        let payload_len = read_u32(&data[16..]) as usize;
        let overhead = LEP_HEADER_SIZE
            .checked_add(metadata_size)
            .and_then(|value| value.checked_add(4))
            .and_then(|value| value.checked_add(authentication_size))
            .ok_or(DecodeError::Length)?;
        let total = overhead
            .checked_add(payload_len)
            .ok_or(DecodeError::Length)?;
        if data.len() != total {
            return Err(DecodeError::Length);
        }
        if read_u32(&data[20..]) != crc32(&data[..20]) {
            return Err(DecodeError::HeaderCrc);
        }
        let payload_offset = LEP_HEADER_SIZE + metadata_size;
        let payload_end = payload_offset + payload_len;
        let payload = &data[payload_offset..payload_end];
        if read_u32(&data[payload_end..]) != crc32(&data[LEP_HEADER_SIZE..payload_end]) {
            return Err(DecodeError::PayloadCrc);
        }
        if !encrypted {
            validate_tlvs(payload)?;
        }
        Ok(Self {
            version: data[4],
            message_type: data[5],
            architecture: data[6],
            flags,
            sequence: read_u32(&data[8..]),
            event_id: read_u32(&data[12..]),
            payload,
            encrypted,
        })
    }

    pub fn is_truncated(&self) -> bool {
        self.flags & ENVELOPE_TRUNCATED != 0
    }

    pub fn payload(&self) -> &'a [u8] {
        self.payload
    }

    pub fn tlvs(&self) -> TlvIterator<'a> {
        TlvIterator {
            remaining: self.payload,
            failed: false,
            encrypted: self.encrypted,
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
    encrypted: bool,
}
impl<'a> Iterator for TlvIterator<'a> {
    type Item = Result<Tlv<'a>, DecodeError>;
    fn next(&mut self) -> Option<Self::Item> {
        if self.failed {
            return None;
        }
        if self.encrypted {
            self.failed = true;
            return Some(Err(DecodeError::Encrypted));
        }
        if self.remaining.is_empty() {
            return None;
        }
        if self.remaining.len() < 4 {
            self.failed = true;
            return Some(Err(DecodeError::Tlv));
        }
        let field_type = read_u16(self.remaining);
        let length = read_u16(&self.remaining[2..]) as usize;
        if field_type == 0 || self.remaining.len() < 4 + length {
            self.failed = true;
            return Some(Err(DecodeError::Tlv));
        }
        let value = &self.remaining[4..4 + length];
        self.remaining = &self.remaining[4 + length..];
        Some(Ok(Tlv { field_type, value }))
    }
}

fn flags_valid(flags: u8) -> bool {
    if flags & !ENVELOPE_KNOWN_FLAGS != 0 {
        return false;
    }
    let authenticated = flags & ENVELOPE_AUTHENTICATED != 0;
    let encrypted = flags & ENVELOPE_ENCRYPTED != 0;
    let aead = flags & ENVELOPE_AEAD != 0;
    encrypted == aead && (!aead || authenticated)
}

fn validate_tlvs(mut payload: &[u8]) -> Result<(), DecodeError> {
    while !payload.is_empty() {
        if payload.len() < 4 {
            return Err(DecodeError::Payload);
        }
        let field_type = read_u16(payload);
        let length = read_u16(&payload[2..]) as usize;
        if field_type == 0 || payload.len() < 4 + length {
            return Err(DecodeError::Payload);
        }
        payload = &payload[4 + length..];
    }
    Ok(())
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

    const BASIC_VECTOR: [u8; 34] = [
        0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
        0x00, 0x06, 0x00, 0x00, 0x00, 0x74, 0xdd, 0xc4, 0x89, 0x01, 0x00, 0x02, 0x00, 0xaa, 0xbb,
        0xea, 0x84, 0xcc, 0xd8,
    ];

    #[test]
    fn crc_vector() {
        assert_eq!(crc32(b"123456789"), 0xcbf4_3926);
    }

    #[test]
    fn parses_shared_golden_vector() {
        let envelope = Envelope::parse(&BASIC_VECTOR).unwrap();
        assert_eq!(envelope.version, LEP_VERSION_1);
        assert_eq!(envelope.sequence, 7);
        assert_eq!(envelope.event_id, 9);
        let tlv = envelope.tlvs().next().unwrap().unwrap();
        assert_eq!(tlv.field_type, 1);
        assert_eq!(tlv.value, &[0xaa, 0xbb]);
    }

    #[test]
    fn parses_authenticated_v1_layout() {
        let payload = [1u8, 0, 2, 0, 0xaa, 0xbb];
        let mut data = [0u8; 66];
        data[..4].copy_from_slice(&0x5054_534cu32.to_le_bytes());
        data[4] = LEP_VERSION_1;
        data[5] = 2;
        data[7] = ENVELOPE_AUTHENTICATED;
        data[16..20].copy_from_slice(&(payload.len() as u32).to_le_bytes());
        let header_crc = crc32(&data[..20]);
        data[20..24].copy_from_slice(&header_crc.to_le_bytes());
        data[24..30].copy_from_slice(&payload);
        data[30..34].copy_from_slice(&crc32(&payload).to_le_bytes());

        let envelope = Envelope::parse(&data).unwrap();
        assert_eq!(envelope.payload(), payload);
    }

    #[test]
    fn parses_aead_layout_without_exposing_ciphertext_as_tlvs() {
        let ciphertext = [0x44u8, 0x55, 0x66, 0x77, 0x88, 0x99];
        let mut data = [0u8; 78];
        data[..4].copy_from_slice(&0x5054_534cu32.to_le_bytes());
        data[4] = LEP_VERSION_1;
        data[5] = 2;
        data[7] = ENVELOPE_AUTHENTICATED | ENVELOPE_ENCRYPTED | ENVELOPE_AEAD;
        data[16..20].copy_from_slice(&(ciphertext.len() as u32).to_le_bytes());
        let header_crc = crc32(&data[..20]);
        data[20..24].copy_from_slice(&header_crc.to_le_bytes());
        data[52..58].copy_from_slice(&ciphertext);
        let payload_crc = crc32(&data[24..58]);
        data[58..62].copy_from_slice(&payload_crc.to_le_bytes());

        let envelope = Envelope::parse(&data).unwrap();
        assert_eq!(envelope.payload(), ciphertext);
        assert!(matches!(
            envelope.tlvs().next(),
            Some(Err(DecodeError::Encrypted))
        ));
    }

    #[test]
    fn rejects_unknown_flags_and_malformed_tlvs() {
        let mut flags = BASIC_VECTOR;
        flags[7] = 0x80;
        let flags_crc = crc32(&flags[..20]);
        flags[20..24].copy_from_slice(&flags_crc.to_le_bytes());
        assert!(matches!(Envelope::parse(&flags), Err(DecodeError::Flags)));

        let mut malformed = BASIC_VECTOR;
        malformed[24] = 0;
        let malformed_crc = crc32(&malformed[24..30]);
        malformed[30..34].copy_from_slice(&malformed_crc.to_le_bytes());
        assert!(matches!(
            Envelope::parse(&malformed),
            Err(DecodeError::Payload)
        ));
    }

    #[test]
    fn reports_truncation() {
        let mut data = BASIC_VECTOR;
        data[7] = ENVELOPE_TRUNCATED;
        let header_crc = crc32(&data[..20]);
        data[20..24].copy_from_slice(&header_crc.to_le_bytes());
        assert!(Envelope::parse(&data).unwrap().is_truncated());
    }
}
