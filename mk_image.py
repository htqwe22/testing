#!/usr/bin/python
# -*- coding: UTF-8 -*-

import struct
import ctypes
import io
import string


class KvUtil(object):
    @classmethod
    def hexstring_to_bin(cls, hex_str):
        return bytes.fromhex(hex_str)

    @classmethod
    def bin_to_hexstring(cls, byte_array):
        return  "".join(format(x, "02X") for x in byte_array)

    @classmethod
    def debug_data(cls, data_name, byte_arr):
        print (data_name, "(%d):" %len(byte_arr), cls.bin_to_hexstring(byte_arr))

def fill_default(file, fill_len, def_byte):
    data = [byte(def_byte)] *1024
    while fill_len > 1024:
        file.write(default_byte)
        fill_len -= 1024
    if fill_len > 0:
        file.write(default_byte, fill_len)


def copy_file(file, bin_name):
    length = 0
    with open(bin_name, "rb") as bin_file:
        while True:
            data = bin_file.read(2048)
            if not data:
                break
            length += data.length
            bin_file.write(data, data.length)
        bin_file.close()
    return length

def mk_image(ver, boot, app):
    length = 0
    with open("z3_v%s.token" %(ver), "wb") as file:
        length = copy_file(file, boot)
        fill_default(file, 32*1024-length, 0xFF)
        length = 32 * 1024
        length += copy_file(file, app)

        b = io.BytesIO()
        b.write(int(4).to_bytes(4, "big"))

    return length


def main():
    mk_image("1.0", z3_boot.bin, z3_app)

if __name__ == '__main__':
    main()