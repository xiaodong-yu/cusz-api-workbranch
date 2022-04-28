/**
 * @file compressor.hh
 * @author Jiannan Tian
 * @brief
 * @version 0.3
 * @date 2022-04-23
 *
 * (C) 2022 by Washington State University, Argonne National Laboratory
 *
 */

#ifndef CUSZ_COMPRESSOR_HH
#define CUSZ_COMPRESSOR_HH

#include <memory>

#include <cuda_runtime.h>

#include "binding.hh"
#include "common/type_traits.hh"
#include "components.hh"
#include "context.hh"
#include "header.hh"

#define PUBLIC_TYPES                                                   \
    using Predictor     = typename BINDING::PREDICTOR;                 \
    using Spcodec       = typename BINDING::SPCODEC;                   \
    using Codec         = typename BINDING::CODEC;                     \
    using FallbackCodec = typename BINDING::FALLBACK_CODEC;            \
    using BYTE          = uint8_t;                                     \
                                                                       \
    using T    = typename Predictor::Origin;                           \
    using FP   = typename Predictor::Precision;                        \
    using E    = typename Predictor::ErrCtrl;                          \
    using H    = typename Codec::Encoded;                              \
    using M    = typename Codec::MetadataT;                            \
    using H_FB = typename FallbackCodec::Encoded;                      \
                                                                       \
    using TimeRecord   = std::vector<std::tuple<const char*, double>>; \
    using timerecord_t = TimeRecord*;

namespace cusz {

template <class BINDING>
class Compressor {
   public:
    PUBLIC_TYPES

   public:
    class impl;
    std::unique_ptr<impl> const pimpl;

   public:
    ~Compressor();
    Compressor();
    Compressor(const Compressor&) = delete;
    // Compressor& operator=(const Compressor&) = delete;
    Compressor(Compressor&&) = delete;
    // Compressor& operator=(Compressor&&) = delete;

    void init(Context*, bool dbg_print = false);
    void init(Header*, bool dbg_print = false);
    void destroy();

    void compress(Context*, T*, BYTE*&, size_t&, cudaStream_t = nullptr, bool = false);
    void decompress(Header*, BYTE*, T*, cudaStream_t = nullptr, bool = true);
    void clear_buffer();
};

template <class BINDING>
class Compressor<BINDING>::impl {
   public:
    PUBLIC_TYPES

   private:
    // state
    bool  use_fallback_codec{false};
    bool  fallback_codec_allocated{false};
    BYTE* d_reserved_compressed{nullptr};
    // profiling
    TimeRecord timerecord;
    // header
    using HEADER = cuszHEADER;
    HEADER header;
    // components
    Predictor*     predictor;
    Spcodec*       spcodec;
    Codec*         codec;
    FallbackCodec* fb_codec;
    // variables
    uint32_t* d_freq;
    float     time_hist;
    dim3      data_len3;

   public:
    ~impl();
    impl();
    impl(const impl&) = delete;
    // impl& impl(const impl&) = delete;
    impl(impl&&) = delete;
    // impl& impl(impl&&)      = delete;

    // public methods
    void init(Context* config, bool dbg_print = false);
    void init(Header* config, bool dbg_print = false);
    void compress(Context*, T*, BYTE*&, size_t&, cudaStream_t = nullptr, bool = false);
    void decompress(Header*, BYTE*, T*, cudaStream_t = nullptr, bool = true);
    void clear_buffer();

    // getter
    void     export_header(HEADER&);
    void     export_timerecord(TimeRecord*);
    uint32_t get_len_data();

   private:
    // helper
    template <class CONFIG>
    void init_detail(CONFIG*, bool);
    void init_codec(size_t, unsigned int, int, int, bool);
    void collect_compress_timerecord();
    void collect_decompress_timerecord();
    void encode_with_exception(E*, size_t, uint32_t*, int, int, int, bool, BYTE*&, size_t&, cudaStream_t, bool);
    void subfile_collect(T*, size_t, BYTE*, size_t, BYTE*, size_t, cudaStream_t, bool);
    void destroy();
    // getter
};

template <typename InputData = float>
struct Framework {
    using DATA    = InputData;                          // depend on template input
    using ERRCTRL = ErrCtrlTrait<4, true>::type;        // predefined
    using FP      = FastLowPrecisionTrait<true>::type;  // predefined
    using Huff4   = HuffTrait<4>::type;
    using Huff8   = HuffTrait<8>::type;
    using Meta4   = MetadataTrait<4>::type;

    /* Predictor */
    using PredictorLorenzo = typename cusz::api::PredictorLorenzo<DATA, ERRCTRL, FP>::impl;
    using PredictorSpline3 = typename cusz::api::PredictorSpline3<DATA, ERRCTRL, FP>::impl;

    /* Lossless Spcodec */
    using SpcodecMat = typename cusz::api::SpCodecCSR<DATA, Meta4>::impl;
    using SpcodecVec = typename cusz::api::SpCodecVec<DATA, Meta4>::impl;

    /* Lossless Codec*/
    using CodecHuffman32 = cusz::api::HuffmanCoarse<ERRCTRL, Huff4, Meta4>::impl;
    using CodecHuffman64 = cusz::api::HuffmanCoarse<ERRCTRL, Huff8, Meta4>::impl;

    /* Predefined Combination */
    using LorenzoFeatured = CompressorTemplate<PredictorLorenzo, SpcodecVec, CodecHuffman32, CodecHuffman64>;
    using Spline3Featured = CompressorTemplate<PredictorSpline3, SpcodecVec, CodecHuffman32, CodecHuffman64>;

    /* Usable Compressor */
    using DefaultCompressor         = class Compressor<LorenzoFeatured>::impl;
    using LorenzoFeaturedCompressor = class Compressor<LorenzoFeatured>::impl;
    using Spline3FeaturedCompressor = class Compressor<Spline3Featured>::impl; /* in progress */
};

}  // namespace cusz

#undef PUBLIC_TYPES

#endif
