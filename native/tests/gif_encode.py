"""Small dependency-free GIF89a writer for native renderer capture sequences.

One shared palette avoids flickering palettes. Reserved source UI colors stay
exact, with weighted median-cut colors for the animated shader layer.
"""
from collections import Counter
from math import sqrt
import struct


def histogram(rgb):
    # Sampling alternate pixels is sufficient to choose a palette; all pixels
    # are encoded below. Quantized histogram bins bound palette-building work.
    return Counter((r >> 3, g >> 3, b >> 3)
                   for r, g, b in zip(rgb[0::6], rgb[1::6], rgb[2::6]))


class Palette:
    def __init__(self, counts, reserved):
        self.colors = list(dict.fromkeys(reserved))
        if len(self.colors) >= 256:
            raise ValueError('Source UI uses too many colors for a GIF palette')
        self.exact = {color: i for i, color in enumerate(self.colors)}
        boxes = [list(counts.items())]

        def score(box):
            ranges = [max(c[axis] for c, n in box) - min(c[axis] for c, n in box)
                      for axis in range(3)]
            return max(ranges) * sqrt(sum(n for c, n in box))

        while len(boxes) < 256 - len(self.colors):
            candidates = [(score(box), i) for i, box in enumerate(boxes) if len(box) > 1]
            if not candidates:
                break
            _, index = max(candidates)
            box = boxes.pop(index)
            axis = max(range(3), key=lambda a: max(c[a] for c, n in box) - min(c[a] for c, n in box))
            box.sort(key=lambda item: item[0][axis])
            halfway = sum(n for c, n in box) / 2
            weight, split = 0, 1
            for j, (color, count) in enumerate(box[:-1]):
                weight += count
                split = j + 1
                if weight >= halfway:
                    break
            boxes.extend((box[:split], box[split:]))
        for box in boxes:
            total = sum(n for c, n in box)
            self.colors.append(tuple(round(sum((c[a] * 8 + 3.5) * n for c, n in box) / total)
                                     for a in range(3)))
        self.lookup = [-1] * 32768
        self.table = bytes(component for color in self.colors for component in color)
        self.table += bytes(768 - len(self.table))

    def indices(self, rgb):
        output = bytearray()
        colors, lookup, exact = self.colors, self.lookup, self.exact
        for color in zip(rgb[0::3], rgb[1::3], rgb[2::3]):
            index = exact.get(color)
            if index is None:
                r, g, b = color
                key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
                index = lookup[key]
                if index < 0:
                    r, g, b = (r & 248) + 3.5, (g & 248) + 3.5, (b & 248) + 3.5
                    index = min(range(len(colors)), key=lambda i:
                                (colors[i][0] - r) ** 2 + (colors[i][1] - g) ** 2 + (colors[i][2] - b) ** 2)
                    lookup[key] = index
            output.append(index)
        return output


def _lzw(pixels):
    output = bytearray()
    pending, bits, width, decoder_next, first = 0, 0, 9, 258, True

    def emit(code):
        nonlocal pending, bits, width, decoder_next, first
        pending |= code << bits
        bits += width
        while bits >= 8:
            output.append(pending & 255)
            pending >>= 8
            bits -= 8
        # Track code widths from the decoder's point of view: its dictionary
        # insertion trails the encoder's by one emitted code.
        if code == 256:
            width, decoder_next, first = 9, 258, True
        elif code != 257:
            if first:
                first = False
            elif decoder_next < 4096:
                decoder_next += 1
                if decoder_next == 1 << width and width < 12:
                    width += 1

    emit(256)
    dictionary, next_code = {}, 258
    prefix = pixels[0]
    for symbol in pixels[1:]:
        key = (prefix << 8) | symbol
        found = dictionary.get(key)
        if found is not None:
            prefix = found
            continue
        emit(prefix)
        if next_code < 4096:
            dictionary[key] = next_code
            next_code += 1
        else:
            emit(256)
            dictionary.clear()
            next_code = 258
        prefix = symbol
    emit(prefix)
    emit(257)
    if bits:
        output.append(pending & 255)
    return output


def write_gif(path, width, height, frames, palette, delay=5):
    """Write full frames; delay is in hundredths of a second, looping forever."""
    count = 0
    with path.open('wb') as out:
        out.write(b'GIF89a' + struct.pack('<HHBBB', width, height, 0xf7, 0, 0))
        out.write(palette.table)
        out.write(b'\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00')
        for pixels in frames:
            if len(pixels) != width * height:
                raise ValueError('GIF frame has incorrect dimensions')
            out.write(b'\x21\xf9\x04\x04' + struct.pack('<H', delay) + b'\x00\x00')
            out.write(b'\x2c' + struct.pack('<HHHHB', 0, 0, width, height, 0))
            out.write(b'\x08')
            packed = _lzw(pixels)
            for at in range(0, len(packed), 255):
                block = packed[at:at + 255]
                out.write(bytes((len(block),)) + block)
            out.write(b'\x00')
            count += 1
        out.write(b'\x3b')
    return count
