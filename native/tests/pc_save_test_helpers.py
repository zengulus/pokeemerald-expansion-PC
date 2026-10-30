"""Helpers for testing generation recovery and internal archive validation."""
import struct
import zlib


def _layout(data):
    if data[:8] != b'RHHPCS01' or len(data) < 80:
        raise ValueError('not a PC save')
    lengths = [struct.unpack_from('<Q', data, 24 + 24 * i)[0] for i in range(2)]
    shared = 80 + sum(lengths)
    if shared + 40 > len(data):
        raise ValueError('truncated PC save')
    specials = [struct.unpack_from('<Q', data, shared + 16 + 8 * i)[0] for i in range(3)]
    extension = shared + 40 + sum(specials)
    if extension > len(data) or struct.unpack_from('<I', data, shared)[0] != len(data) - extension:
        raise ValueError('invalid shared data')
    return lengths, shared, extension


def extension_offset(data):
    return _layout(data)[2]


def update_checksums(data):
    """Refresh outer checksums after a same-size internal mutation."""
    _, shared, _ = _layout(data)
    struct.pack_into('<I', data, 72, zlib.crc32(data[shared:]))
    struct.pack_into('<I', data, 76, zlib.crc32(data[:76]))
    return data


def replace_extension(data, extension):
    """Replace the shared extension and repair its enclosing PC metadata."""
    _, shared, start = _layout(data)
    result = bytearray(data[:start]) + bytearray(extension)
    struct.pack_into('<I', result, shared, len(extension))
    struct.pack_into('<Q', result, 64, len(result) - shared)
    return update_checksums(result)


def replace_game_generations(data, source):
    """Restore an earlier game's generations while keeping current shared data."""
    _, shared, _ = _layout(data)
    _, old_shared, _ = _layout(source)
    header = bytearray(data[:80])
    header[16:64] = source[16:64]
    result = header + bytearray(source[80:old_shared]) + bytearray(data[shared:])
    return update_checksums(result)
