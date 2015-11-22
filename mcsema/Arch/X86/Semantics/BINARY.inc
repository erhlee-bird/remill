/* Copyright 2015 Peter Goodman (peter@trailofbits.com), all rights reserved. */

#include <functional>

namespace {

template <typename D, typename S1, typename S2>
DEF_SEM(ADD, D dst, S1 src1_, S2 src2_) {
  typedef typename BaseType<S1>::Type T;  // `D` might be wider than `S1`.
  const T src1 = R(src1_);
  const T src2 = R(src2_);
  const T res = src1 + src2;
  SET_AFLAGS_OSZAPC(src1, src2, res);
  W(dst) = res;
}

template <typename D, typename S1, typename S2>
DEF_SEM(SUB, D dst, S1 src1_, S2 src2_) {
  typedef typename BaseType<S1>::Type T;  // `D` might be wider than `S1`.
  const T src1 = R(src1_);
  const T src2 = R(src2_);
  const T res = src1 - src2;
  SET_AFLAGS_OSZAPC(src1, src2, res);
  W(dst) = res;
}

template <typename S1, typename S2>
DEF_SEM(CMP, S1 src1_, S2 src2_) {
  typedef typename BaseType<S1>::Type T;
  const T src1 = R(src1_);
  const T src2 = R(src2_);
  const T res = src1 - src2;
  SET_AFLAGS_OSZAPC(src1, src2, res);
}

// Creates signed or unsigned multipliers. The `Converter` template template
// parameter is used to take on integer type and convert it to its signed
// counterpart.
template <template <typename> class Converter>
struct DivMul {

  // 2-operand and 3-operand multipliers truncate their results down to their
  // base types.
  template <typename D, typename S1, typename S2>
  DEF_SEM(MUL, D dst, S1 src1_, S2 src2_) {
    typedef typename BaseType<D>::Type T;
    typedef typename Converter<T>::Type ST;
    const auto src1 = static_cast<ST>(R(src1_));
    const auto src2 = static_cast<ST>(R(src2_));
    const ST res = src1 * src2;
    SET_AFLAGS_OSC(src1, src2, res);
    W(dst) = static_cast<T>(res);
  }

  // `MUL8` and `IMUL8` of `AL` doesn't update `RDX`.
  template <typename S1>
  DEF_SEM(MULA_8, S1 val) {
    typedef typename BaseType<S1>::Type T;
    typedef typename Converter<T>::Type ConvT;

    typedef typename NextLargerIntegerType<T>::Type WideT;
    typedef typename Converter<WideT>::Type ConvWideT;

    const ConvWideT src1 = static_cast<ConvT>(state.gpr.rax.byte.low);
    const ConvWideT src2 = static_cast<ConvT>(R(val));
    const ConvWideT res = src1 * src2;
    SET_AFLAGS_OSC(src1, src2, res);
    W(state.gpr.rax.word) = static_cast<WideT>(res);
  }

#define MAKE_MULTIPLIER(size, read_sel, write_sel) \
    template <typename S2> \
    DEF_SEM(MULAD_ ## size, S2 src2_) { \
      typedef typename BaseType<S2>::Type T; \
      typedef typename Converter<T>::Type ConvT; \
      \
      typedef typename NextLargerIntegerType<T>::Type WideT; \
      typedef typename Converter<WideT>::Type ConvWideT; \
      \
      const ConvWideT src1 = static_cast<ConvT>(state.gpr.rax.read_sel); \
      const ConvWideT src2 = static_cast<ConvT>(R(src2_)); \
      const ConvWideT res = src1 * src2; \
      SET_AFLAGS_OSC(src1, src2, res); \
      W(state.gpr.rdx.write_sel) = static_cast<T>(res >> size); \
      W(state.gpr.rax.write_sel) = static_cast<T>(res); \
    }

MAKE_MULTIPLIER(16, word, word)
MAKE_MULTIPLIER(32, dword, full)
IF_64BIT(MAKE_MULTIPLIER(64, full, full))

#undef MAKE_MULTIPLIER

  // `MUL8` and `IMUL8` of `AL` doesn't update `RDX`.
  template <typename S2>
  DEF_SEM(DIVA_8, S2 src2_) {
    CLEAR_AFLAGS();

    typedef typename BaseType<S2>::Type T;
    typedef typename NextLargerIntegerType<T>::Type WideT;

    typedef typename Converter<T>::Type ConvT;
    typedef typename NextLargerIntegerType<T>::Type ConvWideT;

    const ConvWideT src1 = static_cast<ConvT>(state.gpr.rax.word);
    const ConvWideT src2 = static_cast<ConvT>(R(src2_));
    const auto quot = static_cast<ConvT>(src1 / src2);
    const auto rem = static_cast<ConvT>(src1 % src2);
    W(state.gpr.rax.byte.low) = static_cast<T>(quot);
    W(state.gpr.rax.byte.high) = static_cast<T>(rem);
  }

#define MAKE_DIVIDER(size, read_sel, write_sel) \
    template <typename S2> \
    DEF_SEM(DIVA_ ## size, S2 src2_) { \
      CLEAR_AFLAGS(); \
      typedef typename BaseType<S2>::Type T; \
      typedef typename Converter<T>::Type ConvT; \
      \
      typedef typename NextLargerIntegerType<T>::Type WideT; \
      typedef typename Converter<WideT>::Type ConvWideT; \
      \
      const auto src1_low = static_cast<WideT>(state.gpr.rax.read_sel); \
      const auto src1_high = static_cast<WideT>(state.gpr.rdx.read_sel); \
      const auto src1 = static_cast<ConvWideT>((src1_high << size) | src1_low);\
      const ConvWideT src2 = static_cast<ConvT>(R(src2_)); \
      \
      const auto quot = static_cast<ConvT>(src1 / src2); \
      const auto rem = static_cast<ConvT>(src1 % src2); \
      W(state.gpr.rax.write_sel) = static_cast<T>(quot); \
      W(state.gpr.rdx.write_sel) = static_cast<T>(rem); \
    }

MAKE_DIVIDER(16, word, word)
MAKE_DIVIDER(32, dword, full)
IF_64BIT( MAKE_DIVIDER(64, full, full) )

#undef MAKE_DIVIDER
};

}  // namespace

DEF_ISEL(ADD_MEMb_IMMb_80r0_8) = ADD<M8W, M8, I8>;
DEF_ISEL(ADD_GPR8_IMMb_80r0_8) = ADD<R8W, R8, I8>;
DEF_ISEL_MnW_Mn_In(ADD_MEMv_IMMz, ADD);
DEF_ISEL_RnW_Rn_In(ADD_GPRv_IMMz, ADD);
DEF_ISEL(ADD_MEMb_IMMb_82r0_8) = ADD<M8W, M8, I8>;
DEF_ISEL(ADD_GPR8_IMMb_82r0_8) = ADD<R8W, R8, I8>;
DEF_ISEL_MnW_Mn_In(ADD_MEMv_IMMb, ADD);
DEF_ISEL_RnW_Rn_In(ADD_GPRv_IMMb, ADD);
DEF_ISEL(ADD_MEMb_GPR8_8) = ADD<M8W, M8, R8>;
DEF_ISEL(ADD_GPR8_GPR8_00_8) = ADD<R8W, R8, R8>;
DEF_ISEL_MnW_Mn_Rn(ADD_MEMv_GPRv, ADD);
DEF_ISEL_RnW_Rn_Rn(ADD_GPRv_GPRv_01, ADD);
DEF_ISEL(ADD_GPR8_MEMb_8) = ADD<R8W, R8, M8>;
DEF_ISEL(ADD_GPR8_GPR8_02_8) = ADD<R8W, R8, R8>;
DEF_ISEL_RnW_Rn_Mn(ADD_GPRv_MEMv, ADD);
DEF_ISEL_RnW_Rn_Rn(ADD_GPRv_GPRv_03, ADD);
DEF_ISEL(ADD_AL_IMMb) = ADD<R8W, R8, I8>;
DEF_ISEL_RnW_Rn_In(ADD_OrAX_IMMz, ADD);

DEF_ISEL(SUB_MEMb_IMMb_80r5_8) = SUB<M8W, M8, I8>;
DEF_ISEL(SUB_GPR8_IMMb_80r5_8) = SUB<R8W, R8, I8>;
DEF_ISEL_MnW_Mn_In(SUB_MEMv_IMMz, SUB);
DEF_ISEL_RnW_Rn_In(SUB_GPRv_IMMz, SUB);
DEF_ISEL(SUB_MEMb_IMMb_82r5_8) = SUB<M8W, M8, I8>;
DEF_ISEL(SUB_GPR8_IMMb_82r5_8) = SUB<R8W, R8, I8>;
DEF_ISEL_MnW_Mn_In(SUB_MEMv_IMMb, SUB);
DEF_ISEL_RnW_Rn_In(SUB_GPRv_IMMb, SUB);
DEF_ISEL(SUB_MEMb_GPR8_8) = SUB<M8W, M8, I8>;
DEF_ISEL(SUB_GPR8_GPR8_28_8) = SUB<R8W, R8, R8>;
DEF_ISEL_MnW_Mn_Rn(SUB_MEMv_GPRv, SUB);
DEF_ISEL_RnW_Rn_Rn(SUB_GPRv_GPRv_29, SUB);
DEF_ISEL(SUB_GPR8_GPR8_2A_8) = SUB<R8W, R8, R8>;
DEF_ISEL(SUB_GPR8_MEMb_8) = SUB<R8W, R8, M8>;
DEF_ISEL_RnW_Rn_Rn(SUB_GPRv_GPRv_2B, SUB);
DEF_ISEL_RnW_Rn_Mn(SUB_GPRv_MEMv, SUB);
DEF_ISEL(SUB_AL_IMMb_8) = SUB<R8W, R8, I8>;
DEF_ISEL_RnW_Rn_In(SUB_OrAX_IMMz, SUB);

DEF_ISEL(IMUL_MEMb_8) = DivMul<SignedIntegerType>::MULA_8<M8>;
DEF_ISEL(IMUL_GPR8_8) = DivMul<SignedIntegerType>::MULA_8<R8>;
DEF_ISEL(IMUL_MEMv_8) = DivMul<SignedIntegerType>::MULA_8<M8>;
DEF_ISEL(IMUL_MEMv_16) = DivMul<SignedIntegerType>::MULAD_16<M16>;
DEF_ISEL(IMUL_MEMv_32) = DivMul<SignedIntegerType>::MULAD_32<M32>;
IF_64BIT(DEF_ISEL(IMUL_MEMv_64) = DivMul<SignedIntegerType>::MULAD_64<M64>;)
DEF_ISEL(IMUL_GPRv_8) = DivMul<SignedIntegerType>::MULA_8<R8>;
DEF_ISEL(IMUL_GPRv_16) = DivMul<SignedIntegerType>::MULAD_16<R16>;
DEF_ISEL(IMUL_GPRv_32) = DivMul<SignedIntegerType>::MULAD_32<R32>;
IF_64BIT(DEF_ISEL(IMUL_GPRv_64) = DivMul<SignedIntegerType>::MULAD_64<R64>;)

// All dests are registers, albeit different ones from the sources.
DEF_ISEL_RnW_Mn_In(IMUL_GPRv_MEMv_IMMz, DivMul<SignedIntegerType>::MUL);
DEF_ISEL_RnW_Rn_In(IMUL_GPRv_GPRv_IMMz, DivMul<SignedIntegerType>::MUL);
DEF_ISEL_RnW_Mn_In(IMUL_GPRv_MEMv_IMMb, DivMul<SignedIntegerType>::MUL);
DEF_ISEL_RnW_Rn_In(IMUL_GPRv_GPRv_IMMb, DivMul<SignedIntegerType>::MUL);

// Two-operand, but dest is a register so turns into a three-operand.
DEF_ISEL_RnW_Rn_Mn(IMUL_GPRv_MEMv, DivMul<SignedIntegerType>::MUL);
DEF_ISEL_RnW_Rn_Rn(IMUL_GPRv_GPRv, DivMul<SignedIntegerType>::MUL);

DEF_ISEL(MUL_GPR8_8) = DivMul<UnsignedIntegerType>::MULA_8<R8>;
DEF_ISEL(MUL_MEMb_8) = DivMul<UnsignedIntegerType>::MULA_8<M8>;
DEF_ISEL(MUL_MEMv_8) = DivMul<UnsignedIntegerType>::MULA_8<M8>;
DEF_ISEL(MUL_MEMv_16) = DivMul<UnsignedIntegerType>::MULAD_16<M16>;
DEF_ISEL(MUL_MEMv_32) = DivMul<UnsignedIntegerType>::MULAD_32<M32>;
IF_64BIT(DEF_ISEL(MUL_MEMv_64) = DivMul<UnsignedIntegerType>::MULAD_64<M64>;)
DEF_ISEL(MUL_GPRv_8) = DivMul<UnsignedIntegerType>::MULA_8<R8>;
DEF_ISEL(MUL_GPRv_16) = DivMul<UnsignedIntegerType>::MULAD_16<R16>;
DEF_ISEL(MUL_GPRv_32) = DivMul<UnsignedIntegerType>::MULAD_32<R32>;
IF_64BIT(DEF_ISEL(MUL_GPRv_64) = DivMul<UnsignedIntegerType>::MULAD_64<R64>;)

DEF_ISEL(IDIV_MEMb_8) = DivMul<SignedIntegerType>::DIVA_8<M8>;
DEF_ISEL(IDIV_GPR8_8) = DivMul<SignedIntegerType>::DIVA_8<R8>;
DEF_ISEL(IDIV_MEMv_8) = DivMul<SignedIntegerType>::DIVA_8<M8>;
DEF_ISEL(IDIV_MEMv_16) = DivMul<SignedIntegerType>::DIVA_16<M16>;
DEF_ISEL(IDIV_MEMv_32) = DivMul<SignedIntegerType>::DIVA_16<M32>;
IF_64BIT(DEF_ISEL(IDIV_MEMv_64) = DivMul<SignedIntegerType>::DIVA_16<M64>;)
DEF_ISEL(IDIV_GPRv_8) = DivMul<SignedIntegerType>::DIVA_8<R8>;
DEF_ISEL(IDIV_GPRv_16) = DivMul<SignedIntegerType>::DIVA_16<R16>;
DEF_ISEL(IDIV_GPRv_32) = DivMul<SignedIntegerType>::DIVA_16<R32>;
IF_64BIT(DEF_ISEL(IDIV_GPRv_64) = DivMul<SignedIntegerType>::DIVA_16<R64>;)

DEF_ISEL(DIV_MEMb_8) = DivMul<UnsignedIntegerType>::DIVA_8<M8>;
DEF_ISEL(DIV_GPR8_8) = DivMul<UnsignedIntegerType>::DIVA_8<R8>;
DEF_ISEL(DIV_MEMv_8) = DivMul<UnsignedIntegerType>::DIVA_8<M8>;
DEF_ISEL(DIV_MEMv_16) = DivMul<UnsignedIntegerType>::DIVA_16<M16>;
DEF_ISEL(DIV_MEMv_32) = DivMul<UnsignedIntegerType>::DIVA_16<M32>;
IF_64BIT(DEF_ISEL(DIV_MEMv_64) = DivMul<UnsignedIntegerType>::DIVA_16<M64>;)
DEF_ISEL(DIV_GPRv_8) = DivMul<UnsignedIntegerType>::DIVA_8<R8>;
DEF_ISEL(DIV_GPRv_16) = DivMul<UnsignedIntegerType>::DIVA_16<R16>;
DEF_ISEL(DIV_GPRv_32) = DivMul<UnsignedIntegerType>::DIVA_16<R32>;
IF_64BIT(DEF_ISEL(DIV_GPRv_64) = DivMul<UnsignedIntegerType>::DIVA_16<R64>;)

DEF_ISEL(CMP_MEMb_IMMb_80r7_8) = CMP<M8, I8>;
DEF_ISEL(CMP_GPR8_IMMb_80r7_8) = CMP<R8, I8>;
DEF_ISEL_Mn_In(CMP_MEMv_IMMz, CMP);
DEF_ISEL_Rn_In(CMP_GPRv_IMMz, CMP);
DEF_ISEL(CMP_MEMb_IMMb_82r7_8) = CMP<M8, I8>;
DEF_ISEL(CMP_GPR8_IMMb_82r7_8) = CMP<R8, I8>;
DEF_ISEL_Mn_In(CMP_MEMv_IMMb, CMP);
DEF_ISEL_Rn_In(CMP_GPRv_IMMb, CMP);
DEF_ISEL(CMP_MEMb_GPR8_8) = CMP<M8, I8>;
DEF_ISEL(CMP_GPR8_GPR8_38_8) = CMP<R8, R8>;
DEF_ISEL_Mn_In(CMP_MEMv_GPRv, CMP);
DEF_ISEL_Rn_Rn(CMP_GPRv_GPRv_39, CMP);
DEF_ISEL(CMP_GPR8_GPR8_3A_8) = CMP<R8, R8>;
DEF_ISEL(CMP_GPR8_MEMb_8) = CMP<R8, M8>;
DEF_ISEL_Rn_Rn(CMP_GPRv_GPRv_3B, CMP);
DEF_ISEL_Rn_Mn(CMP_GPRv_MEMv, CMP);
DEF_ISEL(CMP_AL_IMMb_8) = CMP<R8, I8>;
DEF_ISEL_Rn_In(CMP_OrAX_IMMz, CMP);

namespace {

template <typename D, typename S>
DEF_SEM(INC, D dst, S src) {
  typedef typename BaseType<S>::Type T;
  const T val1 = R(src);
  const T val2 = 1;
  const T res = val1 + val2;
  SET_AFLAGS_OSZAP(val1, val2, res);
  W(dst) = res;
}

template <typename D, typename S>
DEF_SEM(DEC, D dst, S src) {
  typedef typename BaseType<S>::Type T;
  const T val1 = R(src);
  const T val2 = 1;
  const T res = val1 - val2;
  SET_AFLAGS_OSZAP(val1, val2, res);
  W(dst) = res;
}

template <typename D, typename S>
DEF_SEM(NEG, D dst, S src) {
  typedef typename BaseType<S>::Type T;
  const T val1 = 0;
  const T val2 = R(src);
  const T res = val1 - val2;
  SET_AFLAGS_OSZAP(val1, val2, res);
  state.aflag.cf = !!res;
  W(dst) = res;
}

}  // namespace

DEF_ISEL(INC_MEMb_8) = INC<M8W, M8>;
DEF_ISEL(INC_GPR8_8) = INC<R8W, R8>;
DEF_ISEL_MnW_Mn(INC_MEMv, INC);
DEF_ISEL_RnW_Rn(INC_GPRv_FFr0, INC);
DEF_ISEL_RnW_Rn(INC_GPRv_40, INC);

DEF_ISEL(DEC_MEMb_8) = DEC<M8W, M8>;
DEF_ISEL(DEC_GPR8_8) = DEC<R8W, R8>;
DEF_ISEL_MnW_Mn(DEC_MEMv, DEC);
DEF_ISEL_RnW_Rn(DEC_GPRv_FFr1, DEC);
DEF_ISEL_RnW_Rn(INC_GPRv_48, DEC);

DEF_ISEL(NEG_MEMb_8) = NEG<M8W, M8>;
DEF_ISEL(NEG_GPR8_8) = NEG<R8W, R8>;
DEF_ISEL_MnW_Mn(NEG_MEMv, NEG);
DEF_ISEL_RnW_Rn(NEG_GRPv, NEG);

/*

363 SBB SBB_MEMb_IMMb_80r3 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
364 SBB SBB_GPR8_IMMb_80r3 BINARY BASE I86 ATTRIBUTES: BYTEOP
365 SBB SBB_MEMv_IMMz BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
366 SBB SBB_GPRv_IMMz BINARY BASE I86 ATTRIBUTES: SCALABLE
367 SBB SBB_MEMb_IMMb_82r3 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
368 SBB SBB_GPR8_IMMb_82r3 BINARY BASE I86 ATTRIBUTES: BYTEOP
369 SBB SBB_MEMv_IMMb BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
370 SBB SBB_GPRv_IMMb BINARY BASE I86 ATTRIBUTES: SCALABLE
371 SBB SBB_MEMb_GPR8 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
372 SBB SBB_GPR8_GPR8_18 BINARY BASE I86 ATTRIBUTES: BYTEOP
373 SBB SBB_MEMv_GPRv BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
374 SBB SBB_GPRv_GPRv_19 BINARY BASE I86 ATTRIBUTES: SCALABLE
375 SBB SBB_GPR8_GPR8_1A BINARY BASE I86 ATTRIBUTES: BYTEOP
376 SBB SBB_GPR8_MEMb BINARY BASE I86 ATTRIBUTES: BYTEOP
377 SBB SBB_GPRv_GPRv_1B BINARY BASE I86 ATTRIBUTES: SCALABLE
378 SBB SBB_GPRv_MEMv BINARY BASE I86 ATTRIBUTES: SCALABLE
379 SBB SBB_AL_IMMb BINARY BASE I86 ATTRIBUTES: BYTEOP
380 SBB SBB_OrAX_IMMz BINARY BASE I86 ATTRIBUTES: SCALABLE

1700 ADC ADC_MEMb_IMMb_80r2 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
1701 ADC ADC_GPR8_IMMb_80r2 BINARY BASE I86 ATTRIBUTES: BYTEOP
1702 ADC ADC_MEMv_IMMz BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
1703 ADC ADC_GPRv_IMMz BINARY BASE I86 ATTRIBUTES: SCALABLE
1704 ADC ADC_MEMb_IMMb_82r2 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
1705 ADC ADC_GPR8_IMMb_82r2 BINARY BASE I86 ATTRIBUTES: BYTEOP
1706 ADC ADC_MEMv_IMMb BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
1707 ADC ADC_GPRv_IMMb BINARY BASE I86 ATTRIBUTES: SCALABLE
1708 ADC ADC_MEMb_GPR8 BINARY BASE I86 ATTRIBUTES: BYTEOP HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE
1709 ADC ADC_GPR8_GPR8_10 BINARY BASE I86 ATTRIBUTES: BYTEOP
1710 ADC ADC_MEMv_GPRv BINARY BASE I86 ATTRIBUTES: HLE_ACQ_ABLE HLE_REL_ABLE LOCKABLE SCALABLE
1711 ADC ADC_GPRv_GPRv_11 BINARY BASE I86 ATTRIBUTES: SCALABLE
1712 ADC ADC_GPR8_MEMb BINARY BASE I86 ATTRIBUTES: BYTEOP
1713 ADC ADC_GPR8_GPR8_12 BINARY BASE I86 ATTRIBUTES: BYTEOP
1714 ADC ADC_GPRv_MEMv BINARY BASE I86 ATTRIBUTES: SCALABLE
1715 ADC ADC_GPRv_GPRv_13 BINARY BASE I86 ATTRIBUTES: SCALABLE
1716 ADC ADC_AL_IMMb BINARY BASE I86 ATTRIBUTES: BYTEOP
1717 ADC ADC_OrAX_IMMz BINARY BASE I86 ATTRIBUTES: SCALABLE

 */