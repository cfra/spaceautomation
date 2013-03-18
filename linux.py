# Courtesy of Armin Ronacher, found at StackOverflow

import ctypes
import os

CLOCK_MONOTONIC = 1 # see <linux/time.h>

class _timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_nsec', ctypes.c_long)
    ]

_librt = ctypes.CDLL('librt.so.1', use_errno=True)
_libc = ctypes.CDLL('libc.so.6', use_errno=True)

_clock_gettime = _librt.clock_gettime
_clock_gettime.argtypes = [ctypes.c_int, ctypes.POINTER(_timespec)]

_if_nametoindex = _libc.if_nametoindex
_if_nametoindex.argtype = [ctypes.c_uint, ctypes.c_char_p]

def now():
    t = _timespec()
    if _clock_gettime(CLOCK_MONOTONIC, ctypes.pointer(t)) != 0:
        errno_ = ctypes.get_errno()
        raise OSError(errno_, os.strerror(errno_))
    return t.tv_sec + t.tv_nsec * 1e-9

def if_nametoindex(interfaceName):
    rv = _if_nametoindex(interfaceName)
    if rv == 0:
        errno_= ctypes.get_errno()
        raise OSError(errno_, os.strerror(errno_))
    return rv
