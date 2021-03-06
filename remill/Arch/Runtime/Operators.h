/* Copyright 2015 Peter Goodman (peter@trailofbits.com), all rights reserved. */

#ifndef REMILL_ARCH_RUNTIME_OPERATORS_H_
#define REMILL_ARCH_RUNTIME_OPERATORS_H_

struct Memory;
struct State;

// Something has gone terribly wrong and we need to stop because there is
// an error.
//
// TODO(pag): What happens if there's a signal handler? How should we
//            communicate the error class?
#define StopFailure() \
    do { \
      __remill_error(state, memory, Read(REG_XIP)); \
      __builtin_unreachable(); \
    } while (false)

namespace {

// Read a value directly.
ALWAYS_INLINE static bool _Read(Memory *, bool val) {
  return val;
}

// Read a value directly.
ALWAYS_INLINE static uint8_t _Read(Memory *, uint8_t val) {
  return val;
}

// Read a value directly.
ALWAYS_INLINE static uint16_t _Read(Memory *, uint16_t val) {
  return val;
}

// Read a value directly.
ALWAYS_INLINE static uint32_t _Read(Memory *, uint32_t val) {
  return val;
}

// Read a value directly.
ALWAYS_INLINE static uint64_t _Read(Memory *, uint64_t val) {
  return val;
}

template <typename T>
ALWAYS_INLINE static
T _Read(Memory *, In<T> imm) {
  return static_cast<T>(imm.val);
}

template <typename T>
ALWAYS_INLINE static
T _Read(Memory *, Rn<T> reg) {
  return static_cast<T>(reg.val);
}

template <typename T>
ALWAYS_INLINE static
T _Read(Memory *, RnW<T> reg) {
  return static_cast<T>(*(reg.val_ref));
}

#define MAKE_RVREAD(prefix, size, accessor, type_prefix) \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size (Memory *, RVn<T> reg) { \
      return {.accessor = { \
          {static_cast<type_prefix ## size ## _t>(reg.val)}}}; \
    } \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size (Memory *, RVnW<T> reg) { \
      return {.accessor = { \
          {static_cast<type_prefix ## size ## _t>(*reg.val_ref)}}}; \
    }

MAKE_RVREAD(U, 32, dwords, uint)
MAKE_RVREAD(U, 64, qwords, uint)
MAKE_RVREAD(F, 32, floats, float)
MAKE_RVREAD(F, 64, doubles, float)

#undef MAKE_RVREAD

#define MAKE_VREAD(prefix, size) \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size (Memory *, Vn<T> reg) { \
      return *reinterpret_cast<const T *>(reg.val);\
    } \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size (Memory *, VnW<T> reg) { \
      return *reinterpret_cast<T *>(reg.val_ref);\
    }

MAKE_VREAD(U, 8)
MAKE_VREAD(U, 16)
MAKE_VREAD(U, 32)
MAKE_VREAD(U, 64)
MAKE_VREAD(U, 128)
MAKE_VREAD(U, 256)
MAKE_VREAD(U, 512)
MAKE_VREAD(F, 32)
MAKE_VREAD(F, 64)

#undef MAKE_VREAD

// Make read operators for reading integral values from memory.
#define MAKE_MREAD(size, ret_size, type_prefix, access_suffix) \
    ALWAYS_INLINE static \
    type_prefix ## ret_size ## _t _Read( \
        Memory *&memory, Mn<type_prefix ## size ## _t> op) { \
      return __remill_read_memory_ ## access_suffix (memory, op.addr); \
    } \
    \
    ALWAYS_INLINE static \
    type_prefix ## ret_size ## _t _Read( \
        Memory *&memory, MnW<type_prefix ## size ## _t> op) { \
      return __remill_read_memory_ ## access_suffix (memory, op.addr); \
    }

MAKE_MREAD(8, 8, uint, 8)
MAKE_MREAD(16, 16, uint, 16)
MAKE_MREAD(32, 32, uint, 32)
MAKE_MREAD(64, 64, uint, 64)
MAKE_MREAD(32, 32, float, f32)
MAKE_MREAD(64, 64, float, f64)
MAKE_MREAD(80, 64, float, f80)

#undef MAKE_MREAD

// Basic write form for references.
template <typename T>
ALWAYS_INLINE static
Memory *_Write(Memory *memory, T &dst, T src) {
  dst = src;
  return memory;
}

// Make write operators for writing values to registers.
#define MAKE_RWRITE(type) \
    ALWAYS_INLINE static \
    Memory *_Write( \
        Memory *memory, RnW<type> reg, type val) { \
      *(reg.val_ref) = val; \
      return memory; \
    }

MAKE_RWRITE(uint8_t)
MAKE_RWRITE(uint16_t)
MAKE_RWRITE(uint32_t)
MAKE_RWRITE(uint64_t)
MAKE_RWRITE(float32_t)
MAKE_RWRITE(float64_t)

#undef MAKE_RWRITE

// Make write operators for writing values to memory.
#define MAKE_MWRITE(size, write_size, type_prefix, access_suffix) \
    ALWAYS_INLINE static \
    Memory *_Write( \
        Memory *memory, MnW<type_prefix ## size ## _t> op, \
        type_prefix ## write_size ## _t val) { \
      return __remill_write_memory_ ## access_suffix (\
          memory, op.addr, val); \
    }

MAKE_MWRITE(8, 8, uint, 8)
MAKE_MWRITE(16, 16, uint, 16)
MAKE_MWRITE(32, 32, uint, 32)
MAKE_MWRITE(64, 64, uint, 64)
MAKE_MWRITE(32, 32, float, f32)
MAKE_MWRITE(64, 64, float, f64)
MAKE_MWRITE(80, 64, float, f80)

#undef MAKE_MWRITE

// Make write operators for writing vectors to vector registers.
#define MAKE_MVWRITE(prefix, size, small_prefix, accessor, base_type_prefix) \
    template <typename T> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, MnW<T> mem, T val, VectorTag) { \
      _Pragma("unroll") \
      for (size_t i = 0UL; i < NumVectorElems(val.accessor); ++i) { \
        memory = __remill_write_memory_ ## small_prefix ( \
            memory, \
            mem.addr + (i * sizeof(val.accessor.elems[0])), \
            val.accessor.elems[i]); \
      } \
      return memory; \
    } \
    template <typename T> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, MnW<T> mem, \
        base_type_prefix ## size ## _t val, \
        NumberTag) { \
      memory = __remill_write_memory_ ## small_prefix (memory, mem.addr, val); \
      return memory; \
    }

MAKE_MVWRITE(U, 8, 8, bytes, uint)
MAKE_MVWRITE(U, 16, 16, words, uint)
MAKE_MVWRITE(U, 32, 32, dwords, uint)
MAKE_MVWRITE(U, 64, 64, qwords, uint)
MAKE_MVWRITE(U, 128, 128, dqwords, uint)
MAKE_MVWRITE(F, 32, f32, floats, float)
MAKE_MVWRITE(F, 64, f64, doubles, float)

#undef MAKE_MVWRITE

#define MAKE_RVWRITE(prefix, size, base_type_prefix, accessor) \
    template <typename T, typename U> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, RVnW<T> reg, U val, VectorTag) { \
      *reinterpret_cast<base_type_prefix ## size ## _t *>(reg.val_ref) = \
          val.accessor.elems[0]; \
      return memory; \
    } \
    \
    template <typename T> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, RVnW<T> reg, \
        base_type_prefix ## size ## _t val, \
        NumberTag) { \
      *reinterpret_cast<base_type_prefix ## size ## _t *>(reg.val_ref) = val; \
      return memory; \
    } \
    \
    template <typename T> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, RVnW<T> reg, \
        vec ## size ## _t val, \
        NumberTag) { \
      *reinterpret_cast<base_type_prefix ## size ## _t *>(reg.val_ref) = \
          val.accessor.elems[0]; \
      return memory; \
    }

MAKE_RVWRITE(U, 32, uint, dwords)
MAKE_RVWRITE(U, 64, uint, qwords)
MAKE_RVWRITE(F, 32, float, floats)
MAKE_RVWRITE(F, 64, float, doubles)

#undef MAKE_RVWRITE

// Make write operators for writing vectors to vector registers.
#define MAKE_VWRITE(prefix, size, accessor, base_type_prefix) \
    template <typename T, typename U> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, VnW<T> reg, U val, VectorTag) { \
      enum : size_t { \
        kNumSrcElems = NumVectorElems(val.accessor), \
        kNumDstElems = NumVectorElems( \
            reinterpret_cast<T *>(reg.val_ref)->accessor) \
      }; \
      _Pragma("unroll") \
      for (size_t i = 0UL; i < kNumSrcElems; ++i) { \
        reinterpret_cast<T *>(reg.val_ref)->accessor.elems[i] = \
            val.accessor.elems[i]; \
      } \
      _Pragma("unroll") \
      for (size_t i = kNumSrcElems; i < kNumDstElems; ++i) { \
        reinterpret_cast<T *>(reg.val_ref)->accessor.elems[i] = 0; \
      } \
      return memory; \
    } \
    \
    template <typename T> \
    ALWAYS_INLINE static \
    Memory *_Do ## prefix ## WriteV ## size ( \
        Memory *memory, VnW<T> reg, \
        base_type_prefix ## size ## _t val, \
        NumberTag) { \
      return _Do ## prefix ## WriteV ## size( \
          memory, reg, vec ## size ## _t{.accessor = {{val}}}, VectorTag()); \
    } \
    \
    template <typename T, typename U> \
    ALWAYS_INLINE static \
    Memory *_ ## prefix ## WriteV ## size ( \
        Memory *memory, T dst, U src) { \
      return _Do ## prefix ## WriteV ## size (memory, dst, src, Tag<U>::kTag); \
    }

MAKE_VWRITE(U, 8, bytes, uint)
MAKE_VWRITE(U, 16, words, uint)
MAKE_VWRITE(U, 32, dwords, uint)
MAKE_VWRITE(U, 64, qwords, uint)
MAKE_VWRITE(U, 128, dqwords, uint)
MAKE_VWRITE(F, 32, floats, float)
MAKE_VWRITE(F, 64, doubles, float)

#undef MAKE_VWRITE

// Make read operators for reading vectors to vector registers.
#define MAKE_MVREAD(prefix, size, small_prefix, accessor) \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size ( \
        Memory *memory, Mn<T> mem) { \
      T val; \
      _Pragma("unroll") \
      for (size_t i = 0UL; i < NumVectorElems(val.accessor); ++i) { \
        val.accessor.elems[i] = __remill_read_memory_ ## small_prefix ( \
            memory, \
            mem.addr + (i * sizeof(val.accessor.elems[0])));\
      } \
      return val; \
    } \
    template <typename T> \
    ALWAYS_INLINE static \
    T _ ## prefix ## ReadV ## size ( \
        Memory *memory, MnW<T> mem) { \
      T val; \
      _Pragma("unroll") \
      for (size_t i = 0UL; i < NumVectorElems(val.accessor); ++i) { \
        val.accessor.elems[i] = __remill_read_memory_ ## small_prefix ( \
            memory, \
            mem.addr + (i * sizeof(val.accessor.elems[0])));\
      } \
      return val; \
    }

MAKE_MVREAD(U, 8, 8, bytes)
MAKE_MVREAD(U, 16, 16, words)
MAKE_MVREAD(U, 32, 32, dwords)
MAKE_MVREAD(U, 64, 64, qwords)
MAKE_MVREAD(U, 128, 128, dqwords)
MAKE_MVREAD(F, 32, f32, floats)
MAKE_MVREAD(F, 64, f64, doubles)

#undef MAKE_MVREAD

#define MAKE_WRITE_REF(type) \
    ALWAYS_INLINE static Memory *_Write(Memory *memory, type &ref, type val) { \
      ref = val; \
      return memory; \
    }

MAKE_WRITE_REF(bool)
MAKE_WRITE_REF(uint8_t)
MAKE_WRITE_REF(uint16_t)
MAKE_WRITE_REF(uint32_t)
MAKE_WRITE_REF(uint64_t)
MAKE_WRITE_REF(float32_t)
MAKE_WRITE_REF(float64_t)

#undef MAKE_WRITE_REF

// For the sake of esthetics and hiding the small-step semantics of memory
// operands, we use this macros to implicitly pass in the `memory` operand,
// which we know will be defined in semantics functions.
#define Read(op) _Read(memory, op)

// Write a source value to a destination operand, where the sizes of the
// valyes must match.
#define Write(op, val) \
    do { \
      static_assert( \
          sizeof(typename BaseType<decltype(op)>::BT) == sizeof(val), \
          "Bad write!"); \
      memory = _Write(memory, op, (val)); \
    } while (false)

// Handle writes of N-bit values to M-bit values with N <= M. If N < M then the
// source value will be zero-extended to the dest value type. This is useful
// on x86-64 where writes to 32-bit registers zero-extend to 64-bits. In a
// 64-bit build of Remill, the `R32W` type used in the X86 architecture
// runtime actually aliases `R64W`.
#define WriteZExt(op, val) \
    do { \
      Write(op, ZExtTo<decltype(op)>(val)); \
    } while (false)

#define UWriteV8 WriteV8
#define SWriteV8 WriteV8
#define WriteV8(op, val) \
    do { \
      memory = _UWriteV8(memory, op, (val)); \
    } while (false)

#define UWriteV16 WriteV16
#define SWriteV16 WriteV16
#define WriteV16(op, val) \
    do { \
      memory = _UWriteV16(memory, op, (val)); \
    } while (false)

#define UWriteV32 WriteV32
#define SWriteV32 WriteV32
#define WriteV32(op, val) \
    do { \
      memory = _UWriteV32(memory, op, (val)); \
    } while (false)

#define UWriteV64 WriteV64
#define SWriteV64 WriteV64
#define WriteV64(op, val) \
    do { \
      memory = _UWriteV64(memory, op, (val)); \
    } while (false)

#define UWriteV128 WriteV128
#define SWriteV128 WriteV128
#define WriteV128(op, val) \
    do { \
      memory = _UWriteV128(memory, op, (val)); \
    } while (false)

#define FWriteV32(op, val) \
    do { \
      memory = _FWriteV32(memory, op, (val)); \
    } while (false)

#define FWriteV64(op, val) \
    do { \
      memory = _FWriteV64(memory, op, (val)); \
    } while (false)

#define UReadV8 ReadV8
#define SReadV8 ReadV8
#define ReadV8(op) _UReadV8(memory, op)

#define UReadV16 ReadV16
#define SReadV16 ReadV16
#define ReadV16(op) _UReadV16(memory, op)

#define UReadV32 ReadV32
#define SReadV32 ReadV32
#define ReadV32(op) _UReadV32(memory, op)

#define UReadV64 ReadV64
#define SReadV64 ReadV64
#define ReadV64(op) _UReadV64(memory, op)

#define UReadV128 ReadV128
#define SReadV128 ReadV128
#define ReadV128(op) _UReadV128(memory, op)

#define FReadV32(op) _FReadV32(memory, op)
#define FReadV64(op) _FReadV64(memory, op)

template <typename T>
ALWAYS_INLINE static constexpr
auto ByteSizeOf(T) -> typename IntegerType<T>::UT {
  return static_cast<typename IntegerType<T>::UT>(
      sizeof(typename BaseType<T>::BT));
}

template <typename T>
ALWAYS_INLINE static constexpr
auto BitSizeOf(T) -> typename IntegerType<T>::UT {
  return static_cast<typename IntegerType<T>::UT>(
      sizeof(typename BaseType<T>::BT) * 8);
}

// Convert the input value into an unsigned integer.
template <typename T>
ALWAYS_INLINE static
auto Unsigned(T val) -> typename IntegerType<T>::UT {
  return static_cast<typename IntegerType<T>::UT>(val);
}

// Convert the input value into a signed integer.
template <typename T>
ALWAYS_INLINE static
auto Signed(T val) -> typename IntegerType<T>::ST {
  return static_cast<typename IntegerType<T>::ST>(val);
}

// Return the largest possible value assignable to `val`.
template <typename T>
ALWAYS_INLINE static T Maximize(T val) {
  return std::numeric_limits<T>::max();
}

// Return the smallest possible value assignable to `val`.
template <typename T>
ALWAYS_INLINE static T Minimize(T val) {
  return std::numeric_limits<T>::min();
}

#define MAKE_CONVERT(dest_type, name) \
    template <typename T> \
    ALWAYS_INLINE static \
    dest_type name(T val) { \
      return static_cast<dest_type>(val); \
    }

MAKE_CONVERT(int8_t, Int8)
MAKE_CONVERT(int16_t, Int16)
MAKE_CONVERT(int32_t, Int32)
MAKE_CONVERT(int64_t, Int64)
MAKE_CONVERT(uint8_t, UInt8)
MAKE_CONVERT(uint16_t, UInt16)
MAKE_CONVERT(uint32_t, UInt32)
MAKE_CONVERT(uint64_t, UInt64)
MAKE_CONVERT(float32_t, Float32)
MAKE_CONVERT(float64_t, Float64)

#undef MAKE_CONVERT

// Return the value as-is. This is useful when making many accessors using
// macros, because it lets us decide to pull out values as-is, as unsigned
// integers, or as signed integers.
template <typename T>
ALWAYS_INLINE static
T Identity(T val) {
  return val;
}

// Convert an integer to some other type. This is important for
// integer literals, whose type are `int`.
template <typename T, typename U>
ALWAYS_INLINE static
auto Literal(U val) -> typename IntegerType<T>::BT {
  return static_cast<typename IntegerType<T>::BT>(val);
}

template <typename T, typename U>
ALWAYS_INLINE static
auto ULiteral(U val) -> typename IntegerType<T>::UT {
  return static_cast<typename IntegerType<T>::UT>(val);
}

template <typename T, typename U>
ALWAYS_INLINE static
auto SLiteral(U val) -> typename IntegerType<T>::ST {
  return static_cast<typename IntegerType<T>::ST>(val);
}


// Zero-extend an integer to twice its current width.
template <typename T>
ALWAYS_INLINE static
auto ZExt(T val) -> typename IntegerType<T>::WUT {
  return static_cast<typename IntegerType<T>::WUT>(Unsigned(val));
}

// Zero-extend an integer type explicitly specified by `DT`. This is useful
// for things like writing to a possibly wider version of a register, but
// not knowing exactly how wide the wider version is.
template <typename DT, typename T>
ALWAYS_INLINE static
auto ZExtTo(T val) -> typename IntegerType<DT>::UT {
  typedef typename IntegerType<DT>::UT UT;
  static_assert(sizeof(T) <= sizeof(typename IntegerType<DT>::BT),
                "Bad extension.");
  return static_cast<UT>(Unsigned(val));
}

// Sign-extend an integer to twice its current width.
template <typename T>
ALWAYS_INLINE static
auto SExt(T val) -> typename IntegerType<T>::WST {
  return static_cast<typename IntegerType<T>::WST>(Signed(val));
}

// Zero-extend an integer type explicitly specified by `DT`.
template <typename DT, typename T>
ALWAYS_INLINE static
auto SExtTo(T val) -> typename IntegerType<DT>::ST {
  static_assert(sizeof(T) <= sizeof(typename IntegerType<DT>::BT),
                "Bad extension.");
  return static_cast<typename IntegerType<DT>::ST>(Signed(val));
}

// Truncate an integer to half of its current width.
template <typename T>
ALWAYS_INLINE static
auto Trunc(T val) -> typename NextSmallerIntegerType<T>::BT {
  return static_cast<typename NextSmallerIntegerType<T>::BT>(val);
}

// Truncate an integer to have the same width/sign as the type specified
// by `DT`.
template <typename DT, typename T>
ALWAYS_INLINE static
auto TruncTo(T val) -> typename IntegerType<DT>::BT {
  static_assert(sizeof(T) >= sizeof(typename IntegerType<DT>::BT),
                "Bad truncation.");
  return static_cast<typename IntegerType<DT>::BT>(val);
}

// Useful for stubbing out an operator.
#define MAKE_NOP(...)

// Unary operator.
#define MAKE_UOP(name, type, widen_type, op) \
    ALWAYS_INLINE type name(type R) { \
      return static_cast<type>(op static_cast<widen_type>(R)); \
    }

// Binary operator.
#define MAKE_BINOP(name, type, widen_type, op) \
    ALWAYS_INLINE type name(type L, type R) { \
      return static_cast<type>( \
        static_cast<widen_type>(L) op static_cast<widen_type>(R)); \
    }

#define MAKE_BOOLBINOP(name, type, widen_type, op) \
    ALWAYS_INLINE bool name(type L, type R) { \
      return L op R; \
    }

// The purpose of the widening type is that Clang/LLVM will already extend
// the types of the inputs to their "natural" machine size, so we'll just
// make that explicit, where `addr_t` encodes the natural machine word.
#define MAKE_OPS(name, op, make_int_op, make_float_op) \
    make_int_op(U ## name, uint8_t, addr_t, op) \
    make_int_op(U ## name, uint16_t, addr_t, op) \
    make_int_op(U ## name, uint32_t, addr_t, op) \
    make_int_op(U ## name, uint64_t, uint64_t, op) \
    make_int_op(U ## name, uint128_t, uint128_t, op) \
    make_int_op(S ## name, int8_t, addr_diff_t, op) \
    make_int_op(S ## name, int16_t, addr_diff_t, op) \
    make_int_op(S ## name, int32_t, addr_diff_t, op) \
    make_int_op(S ## name, int64_t, int64_t, op) \
    make_int_op(S ## name, int128_t, int128_t, op) \
    make_float_op(F ## name, float32_t, float32_t, op) \
    make_float_op(F ## name, float64_t, float64_t, op)

MAKE_OPS(Add, +, MAKE_BINOP, MAKE_BINOP)
MAKE_OPS(Sub, -, MAKE_BINOP, MAKE_BINOP)
MAKE_OPS(Mul, *, MAKE_BINOP, MAKE_BINOP)
MAKE_OPS(Div, /, MAKE_BINOP, MAKE_BINOP)
MAKE_OPS(Rem, %, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(And, &, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(AndN, & ~, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(Or, |, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(Xor, ^, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(Shr, >>, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(Shl, <<, MAKE_BINOP, MAKE_NOP)
MAKE_OPS(Neg, -, MAKE_UOP, MAKE_UOP)
MAKE_OPS(Not, ~, MAKE_UOP, MAKE_NOP)

// TODO(pag): Handle unordered and ordered floating point comparisons.
MAKE_OPS(CmpEq, ==, MAKE_BOOLBINOP, MAKE_BOOLBINOP)
MAKE_OPS(CmpNeq, !=, MAKE_BOOLBINOP, MAKE_BOOLBINOP)
MAKE_OPS(CmpLt, <, MAKE_BOOLBINOP, MAKE_BOOLBINOP)
MAKE_OPS(CmpLte, <=, MAKE_BOOLBINOP, MAKE_BOOLBINOP)
MAKE_OPS(CmpGt, >, MAKE_BOOLBINOP, MAKE_BOOLBINOP)
MAKE_OPS(CmpGte, >=, MAKE_BOOLBINOP, MAKE_BOOLBINOP)

#undef MAKE_UNOP
#undef MAKE_BINOP
#undef MAKE_OPS

ALWAYS_INLINE static bool BAnd(bool a, bool b) {
  return a && b;
}

ALWAYS_INLINE static bool BOr(bool a, bool b) {
  return a || b;
}

ALWAYS_INLINE static bool BXor(bool a, bool b) {
  return a != b;
}

ALWAYS_INLINE static bool BXnor(bool a, bool b) {
  return a == b;
}

ALWAYS_INLINE static bool BNot(bool a) {
  return !a;
}

// Binary broadcast operator.
#define MAKE_BIN_BROADCAST(op, size, accessor, in, out) \
    template <typename T> \
    ALWAYS_INLINE static \
    T op ## V ## size(T L, T R) { \
      _Pragma("unroll") \
      for (auto i = 0UL; i < NumVectorElems(L.accessor); ++i) { \
        L.accessor.elems[i] = out(op(in(L.accessor.elems[i]), \
                                     in(R.accessor.elems[i]))); \
      } \
      return L; \
    }

// Unary broadcast operator.
#define MAKE_UN_BROADCAST(op, size, accessor, in, out) \
    template <typename T> \
    ALWAYS_INLINE static \
    T op ## V ## size(T R) { \
      _Pragma("unroll") \
      for (auto i = 0UL; i < NumVectorElems(R.accessor); ++i) { \
        R.accessor.elems[i] = out(op(in(R.accessor.elems[i]))); \
      } \
      return R; \
    }

#define MAKE_BROADCASTS(op, make_int_broadcast, make_float_broadcast) \
    make_int_broadcast(U ## op, 8, bytes, Unsigned, Unsigned) \
    make_int_broadcast(U ## op, 16, words, Unsigned, Unsigned) \
    make_int_broadcast(U ## op, 32, dwords, Unsigned, Unsigned) \
    make_int_broadcast(U ## op, 64, qwords, Unsigned, Unsigned) \
    make_int_broadcast(S ## op, 8, bytes, Signed, Unsigned) \
    make_int_broadcast(S ## op, 16, words, Signed, Unsigned) \
    make_int_broadcast(S ## op, 32, dwords, Signed, Unsigned) \
    make_int_broadcast(S ## op, 64, qwords, Signed, Unsigned) \
    make_float_broadcast(F ## op, 32, floats, Identity, Identity) \
    make_float_broadcast(F ## op, 64, doubles, Identity, Identity) \

MAKE_BROADCASTS(Add, MAKE_BIN_BROADCAST, MAKE_BIN_BROADCAST)
MAKE_BROADCASTS(Sub, MAKE_BIN_BROADCAST, MAKE_BIN_BROADCAST)
MAKE_BROADCASTS(Mul, MAKE_BIN_BROADCAST, MAKE_BIN_BROADCAST)
MAKE_BROADCASTS(Div, MAKE_BIN_BROADCAST, MAKE_BIN_BROADCAST)
MAKE_BROADCASTS(Rem, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(And, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(AndN, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Or, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Xor, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Shl, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Shr, MAKE_BIN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Neg, MAKE_UN_BROADCAST, MAKE_NOP)
MAKE_BROADCASTS(Not, MAKE_UN_BROADCAST, MAKE_NOP)

#undef MAKE_BIN_BROADCAST
#undef MAKE_UN_BROADCAST

// Binary broadcast operator.
#define MAKE_ACCUMULATE(op, size, accessor, in, out) \
    template <typename T> \
    ALWAYS_INLINE static \
    auto Accumulate ## op ## V ## size(T R) \
        -> decltype(out(R.accessor.elems[0])) { \
      auto L = in(R.accessor.elems[0]); \
      _Pragma("unroll") \
      for (auto i = 1UL; i < NumVectorElems(R.accessor); ++i) { \
        L = out(op(L, in(R.accessor.elems[i]))); \
      } \
      return L; \
    }

MAKE_BROADCASTS(Add, MAKE_ACCUMULATE, MAKE_ACCUMULATE)
MAKE_BROADCASTS(And, MAKE_ACCUMULATE, MAKE_NOP)
MAKE_BROADCASTS(AndN, MAKE_ACCUMULATE, MAKE_NOP)
MAKE_BROADCASTS(Or, MAKE_ACCUMULATE, MAKE_NOP)
MAKE_BROADCASTS(Xor, MAKE_ACCUMULATE, MAKE_NOP)

#undef MAKE_ACCUMULATE
#undef MAKE_UN_BROADCAST
#undef MAKE_BROADCASTS
#undef MAKE_NOP

template <size_t n, typename T>
auto NthVectorElem(const T &vec) -> typename VectorType<T>::BaseType {
  static_assert(n <= NumVectorElems(vec),
                "Cannot access beyond end of vector.");
  return vec[n];
}

// Access the Nth element of an aggregate vector.
#define MAKE_EXTRACT(size, base_type, accessor, out, prefix) \
    template <size_t n, typename T> \
    base_type prefix ## ExtractV ## size(const T &vec) { \
      static_assert(n <= NumVectorElems(vec.accessor), \
                    "Cannot access beyond end of vector."); \
      return out(vec.accessor.elems[n]); \
    }

MAKE_EXTRACT(8, uint8_t, bytes, Unsigned, U)
MAKE_EXTRACT(16, uint16_t, words, Unsigned, U)
MAKE_EXTRACT(32, uint32_t, dwords, Unsigned, U)
MAKE_EXTRACT(64, uint64_t, qwords, Unsigned, U)
MAKE_EXTRACT(128, uint128_t, dqwords, Unsigned, U)
MAKE_EXTRACT(8, int8_t, bytes, Signed, S)
MAKE_EXTRACT(16, int16_t, words, Signed, S)
MAKE_EXTRACT(32, int32_t, dwords, Signed, S)
MAKE_EXTRACT(64, int64_t, qwords, Signed, S)
MAKE_EXTRACT(128, int128_t, dqwords, Signed, S)
MAKE_EXTRACT(32, float32_t, floats, Identity, F)
MAKE_EXTRACT(64, float64_t, doubles, Identity, F)
#undef MAKE_EXTRACT

// Access the Nth element of an aggregate vector.
#define MAKE_INSERT(size, base_type, accessor, out, prefix) \
    template <size_t n, typename T> \
    T prefix ## InsertV ## size(T vec, base_type val) { \
      static_assert(n <= NumVectorElems(vec.accessor), \
                    "Cannot access beyond end of vector."); \
      vec.accessor.elems[n] = out(val); \
      return vec; \
    }

MAKE_INSERT(8, uint8_t, bytes, Unsigned, U)
MAKE_INSERT(16, uint16_t, words, Unsigned, U)
MAKE_INSERT(32, uint32_t, dwords, Unsigned, U)
MAKE_INSERT(64, uint64_t, qwords, Unsigned, U)
MAKE_INSERT(128, uint128_t, dqwords, Unsigned, U)
MAKE_INSERT(8, int8_t, bytes, Unsigned, S)
MAKE_INSERT(16, int16_t, words, Unsigned, S)
MAKE_INSERT(32, int32_t, dwords, Unsigned, S)
MAKE_INSERT(64, int64_t, qwords, Unsigned, S)
MAKE_INSERT(128, int128_t, dqwords, Unsigned, S)
MAKE_INSERT(32, float32_t, floats, Identity, F)
MAKE_INSERT(64, float64_t, doubles, Identity, F)
#undef MAKE_EXTRACT

// Binary broadcast operator.
#define MAKE_VCLEAR(prefix, accessor, size) \
    template <typename T> \
    ALWAYS_INLINE static \
    T prefix ## ClearV ## size(T vec) { \
      _Pragma("unroll") \
      for (auto i = 0UL; i < NumVectorElems(vec.accessor); ++i) { \
        vec.accessor.elems[i] = 0; \
      } \
      return vec; \
    }

MAKE_VCLEAR(U, bytes, 8)
MAKE_VCLEAR(U, words, 16)
MAKE_VCLEAR(U, dwords, 32)
MAKE_VCLEAR(U, qwords, 64)
MAKE_VCLEAR(U, dqwords, 128)
MAKE_VCLEAR(F, floats, 32)
MAKE_VCLEAR(F, doubles, 64)

#undef MAKE_VCLEAR

// Esthetically pleasing names that hide the implicit small-step semantics
// of the memory pointer.
#define BarrierLoadLoad() \
    do { \
      memory = __remill_barrier_load_load(memory); \
    } while (false)

#define BarrierLoadStore() \
    do { \
      memory = __remill_barrier_load_store(memory); \
    } while (false)

#define BarrierStoreLoad() \
    do { \
      memory = __remill_barrier_store_load(memory); \
    } while (false)

#define BarrierStoreStore() \
    do { \
      memory = __remill_barrier_store_store(memory); \
    } while (false)

// Make a predicate for querying the type of an operand.
#define MAKE_PRED(name, X, val) \
    template <typename T> \
    ALWAYS_INLINE static constexpr bool Is ## name(X<T>) { \
      return val; \
    }

MAKE_PRED(Register, Rn, true)
MAKE_PRED(Register, RnW, true)
MAKE_PRED(Register, Vn, true)
MAKE_PRED(Register, VnW, true)
MAKE_PRED(Register, Mn, false)
MAKE_PRED(Register, MnW, false)
MAKE_PRED(Register, In, false)

MAKE_PRED(Memory, Rn, false)
MAKE_PRED(Memory, RnW, false)
MAKE_PRED(Memory, Vn, false)
MAKE_PRED(Memory, VnW, false)
MAKE_PRED(Memory, Mn, true)
MAKE_PRED(Memory, MnW, true)
MAKE_PRED(Memory, In, false)

MAKE_PRED(Immediate, Rn, false)
MAKE_PRED(Immediate, RnW, false)
MAKE_PRED(Immediate, Vn, false)
MAKE_PRED(Immediate, VnW, false)
MAKE_PRED(Immediate, Mn, false)
MAKE_PRED(Immediate, MnW, false)
MAKE_PRED(Immediate, In, true)

#undef MAKE_PRED
#define MAKE_PRED(name, T, val) \
    ALWAYS_INLINE static constexpr bool Is ## name(T) { \
      return val; \
    }

MAKE_PRED(Register, uint8_t, true)
MAKE_PRED(Register, uint16_t, true)
MAKE_PRED(Register, uint32_t, true)
MAKE_PRED(Register, uint64_t, true)

MAKE_PRED(Immediate, uint8_t, true)
MAKE_PRED(Immediate, uint16_t, true)
MAKE_PRED(Immediate, uint32_t, true)
MAKE_PRED(Immediate, uint64_t, true)

#undef MAKE_PRED

template <typename T>
ALWAYS_INLINE static Mn<T> GetElementPtr(Mn<T> addr, T index) {
  return {addr.addr + (index * static_cast<addr_t>(ByteSizeOf(addr)))};
}

template <typename T>
ALWAYS_INLINE static MnW<T> GetElementPtr(MnW<T> addr, T index) {
  return {addr.addr + (index * static_cast<addr_t>(ByteSizeOf(addr)))};
}

template <typename T>
ALWAYS_INLINE static
auto ReadPtr(addr_t addr) -> Mn<typename BaseType<T>::BT> {
  return {addr};
}

template <typename T>
ALWAYS_INLINE static
auto ReadPtr(addr_t addr, addr_t seg) -> Mn<typename BaseType<T>::BT> {
  return {__remill_compute_address(addr, seg)};
}

template <typename T>
ALWAYS_INLINE static
auto WritePtr(addr_t addr) -> MnW<typename BaseType<T>::BT> {
  return {addr};
}

template <typename T>
ALWAYS_INLINE static
auto WritePtr(addr_t addr, addr_t seg) -> MnW<typename BaseType<T>::BT> {
  return {__remill_compute_address(addr, seg)};
}

template <typename T>
ALWAYS_INLINE static addr_t AddressOf(Mn<T> addr) {
  return addr.addr;
}

template <typename T>
ALWAYS_INLINE static addr_t AddressOf(MnW<T> addr) {
  return addr.addr;
}

template <typename T>
ALWAYS_INLINE static T Select(bool cond, T if_true, T if_false) {
  return cond ? if_true : if_false;
}

#define BUndefined __remill_undefined_bool
#define UUndefined8 __remill_undefined_8
#define UUndefined16 __remill_undefined_16
#define UUndefined32 __remill_undefined_32
#define UUndefined64 __remill_undefined_64

}  // namespace

#endif  // REMILL_ARCH_RUNTIME_OPERATORS_H_
