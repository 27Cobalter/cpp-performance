#include <iostream>
#include <cassert>
#include <concepts>
#include <format>
#include <limits>
#include <new>
#include <valarray>
#include <chrono>
#include <ranges>
#include <span>

#include <InstructionInfo.h>

#include "lut.h"

template <typename T, typename U>
  requires std::convertible_to<T, float> && std::convertible_to<U, float>
float CalcDiff(T* t, U* u, int32_t data_size) {
  float diff = 0;
  for (auto i : std::views::iota(0, data_size)) {
    diff += (t[i] - u[i]) * (t[i] - u[i]);
    if (diff >= 1.0) {
      // assert(false);
    }
  }
  return diff / data_size;
}

auto main() -> int {
  // constexpr int32_t loop_count = 1000;
  constexpr int32_t loop_count = 1;
  std::valarray<int32_t> width_samples{512};

  using IIIS                = InstructionInfo::InstructionSet;
  const bool supported_avx2       = InstructionInfo::IsSupported(IIIS::AVX2);
  const bool supported_avx512f    = InstructionInfo::IsSupported(IIIS::AVX512F);
  const bool supported_avx512vbmi = InstructionInfo::IsSupported(IIIS::AVX512_VBMI);

  decltype(std::chrono::high_resolution_clock::now()) start, end;
  int32_t time_count;
  float diff;

#ifdef _MSC_VER
  std::shared_ptr<uint16_t[]> src(new uint16_t[width_samples.max() * width_samples.max()]);
  std::shared_ptr<uint8_t[]> dst(new uint8_t[width_samples.max() * width_samples.max()]);
  std::shared_ptr<uint8_t[]> ref(new uint8_t[width_samples.max() * width_samples.max()]);
#else
  std::shared_ptr<uint16_t[]> src(new(std::align_val_t(64)) uint16_t[width_samples.max() * width_samples.max()]);
  std::shared_ptr<uint8_t[]> dst(new(std::align_val_t(64)) uint8_t[width_samples.max() * width_samples.max()]);
  std::shared_ptr<uint8_t[]> ref(new(std::align_val_t(64)) uint8_t[width_samples.max() * width_samples.max()]);
#endif

  // constexpr int32_t LUT_END = static_cast<int32_t>(std::numeric_limits<uint16_t>::max()) + 1;
  constexpr int32_t LUT_END = 0xFFFF + 1;
  LUT lut(LUT_END);

  for (auto width : width_samples) {
    uint16_t* sptr = src.get();
    uint8_t* dptr  = dst.get();
    uint8_t* rptr  = ref.get();
    uint32_t ref_lut[std::numeric_limits<uint16_t>::max() + 1];

    constexpr int32_t lut_min = 0x000;
    constexpr int32_t lut_max = 0x0FF;
    const int32_t data_size   = width * width;

    // create source
    for (auto i : std::views::iota(0, data_size)) {
      sptr[i] = i & std::numeric_limits<uint16_t>::max();
    }

    // create reference
    for (auto i : std::views::iota(0, LUT_END)) {
      ref_lut[i] = std::clamp(
          static_cast<int32_t>(255.0 / (lut_max - lut_min) * (i - lut_min) + 0.5), 0, 255);
    }
    std::cout << std::endl;
    for (auto i : std::views::iota(0, data_size)) {
      rptr[i] = ref_lut[sptr[i]];
    }

    // naive lut create
    std::ranges::fill(std::span(lut.lut_.get(), LUT_END), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Create_Impl<LUT::Method::naive_lut>(lut_min, lut_max);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                             CalcDiff(lut.lut_.get(), ref_lut, LUT_END))
              << std::endl;

    // naive lut convert
    std::ranges::fill(std::span(dptr, data_size), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Convert_Impl<LUT::Method::naive_lut>(sptr, dptr, data_size);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                             CalcDiff(dptr, rptr, data_size))
              << std::endl;

    if (supported_avx2) {
      // avx2 lut create
      std::ranges::fill(std::span(lut.lut_.get(), LUT_END), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Create_Impl<LUT::Method::avx2_lut>(lut_min, lut_max);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(lut.lut_.get(), ref_lut, LUT_END))
                << std::endl;

      // avx2 lut convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx2_lut>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }

    if (supported_avx512f) {
      // avx512 lut convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx512f_lut>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }
    if (supported_avx512vbmi) {
      // avx512 lut convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx512vbmi_lut>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }

    std::cout << "naive calc" << std::endl;
    //      calc create
    std::ranges::fill(std::span(lut.lut_.get(), LUT_END), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Create_Impl<LUT::Method::naive_calc>(lut_min, lut_max);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count, "null")
              << std::endl;

    // naive calc convert
    std::ranges::fill(std::span(dptr, data_size), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Convert_Impl<LUT::Method::naive_calc>(sptr, dptr, data_size);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                             CalcDiff(dptr, rptr, data_size))
              << std::endl;

    if (supported_avx2) {
      // avx2 calc convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx2_calc>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;

      // avx2 calc int weight convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx2_calc_intweight>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }

    if (supported_avx512f) {
      // avx512 calc convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx512f_calc>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }
    if (supported_avx512vbmi) {
      // avx512 calc convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx512vbmi_calc>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;

      // avx512 calc int weight convert
      std::ranges::fill(std::span(dptr, data_size), 0);
      start = std::chrono::high_resolution_clock::now();
      for (auto current_loop : std::views::iota(0, loop_count)) {
        lut.Convert_Impl<LUT::Method::avx512vbmi_calc_intweight>(sptr, dptr, data_size);
      }
      end        = std::chrono::high_resolution_clock::now();
      time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                               CalcDiff(dptr, rptr, data_size))
                << std::endl;
    }

    std::cout << "Auto Impl" << std::endl;
    // Auto Croete
    std::ranges::fill(std::span(lut.lut_.get(), LUT_END), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Create(lut_min, lut_max);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                             CalcDiff(lut.lut_.get(), ref_lut, LUT_END))
              << std::endl;

    // Auto Convert
    std::ranges::fill(std::span(dptr, data_size), 0);
    start = std::chrono::high_resolution_clock::now();
    for (auto current_loop : std::views::iota(0, loop_count)) {
      lut.Convert(sptr, dptr, data_size);
    }
    end        = std::chrono::high_resolution_clock::now();
    time_count = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << std::format("time: {}, diff: {}", time_count / loop_count,
                             CalcDiff(dptr, rptr, data_size))
              << std::endl;

  }

  return 0;
}
