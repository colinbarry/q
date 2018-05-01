/*=============================================================================
   Copyright (c) 2014-2018 Joel de Guzman. All rights reserved.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(CYCFI_Q_PITCH_DETECTOR_HPP_MARCH_12_2018)
#define CYCFI_Q_PITCH_DETECTOR_HPP_MARCH_12_2018

#include <q/bacf.hpp>
#include <array>
#include <utility>

namespace cycfi { namespace q
{
   ////////////////////////////////////////////////////////////////////////////
   template <typename T = std::uint32_t>
   class pitch_detector
   {
   public:

      static constexpr float max_deviation = 0.95;
      static constexpr std::size_t max_harmonics = 5;

                           pitch_detector(
                              frequency lowest_freq
                            , frequency highest_freq
                            , std::uint32_t sps
                            , float threshold
                           );

                           pitch_detector(pitch_detector const& rhs) = default;
                           pitch_detector(pitch_detector&& rhs) = default;

      pitch_detector&      operator=(pitch_detector const& rhs) = default;
      pitch_detector&      operator=(pitch_detector&& rhs) = default;

      bool                 operator()(float s);
      float                frequency() const    { return _frequency; }
      float                periodicity() const;

      bacf<T> const&       bacf() const         { return _bacf; }

   private:

      std::size_t          harmonic() const;
      float                calculate_frequency() const;

      q::bacf<T>           _bacf;
      float                _frequency;
      std::uint32_t        _sps;
      std::size_t          _ticks = 0;
      float                _max_val = 0.0f;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Implementation
   ////////////////////////////////////////////////////////////////////////////
   template <typename T>
   inline pitch_detector<T>::pitch_detector(
       q::frequency lowest_freq
     , q::frequency highest_freq
     , std::uint32_t sps
     , float threshold
   )
     : _bacf(lowest_freq, highest_freq, sps, threshold)
     , _frequency(-1.0f)
     , _sps(sps)
   {}

   template <typename T>
   inline bool pitch_detector<T>::operator()(float s)
   {
      return _bacf(s,
         [this]()
         {
            // auto const& info = _bacf.result();
            // if (info.min_count > info.max_count * (1.0f - min_periodicity))
            //    _frequency = -1.0f;
            // else
               _frequency = calculate_frequency();
         }
      );
   }

   namespace detail
   {
      template <std::size_t harmonic>
      struct find_harmonic_
      {
         template <typename Correlation>
         static std::size_t
         call(
            Correlation const& corr, std::size_t index
          , std::size_t min_period, float threshold
         )
         {
            float delta = float(index) / harmonic;
            float until = index - delta;
            if (delta < min_period)
               return find_harmonic_<harmonic-1>
                  ::call(corr, index, min_period, threshold);

            for (auto i = delta; i < until; i += delta)
               if (corr[i] > threshold)
                  return find_harmonic_<harmonic-1>
                     ::call(corr, index, min_period, threshold);
            return harmonic;
         }
      };

      template <>
      struct find_harmonic_<2>
      {
         template <typename Correlation>
         static std::size_t
         call(
            Correlation const& corr, std::size_t index
          , std::size_t min_period, float threshold
         )
         {
            auto delta = index / 2;
            return (delta < min_period || corr[delta] > threshold)? 1 : 2;
         }
      };
   }

   template <typename T>
   inline std::size_t pitch_detector<T>::harmonic() const
   {
      auto const& info = _bacf.result();
      auto const& corr = info.correlation;
      auto index = info.index;
      auto diff = info.max_count - info.min_count;
      auto threshold = info.max_count - (max_deviation * diff);

      auto min_period = _bacf.minimum_period();
      auto found = detail::find_harmonic_<max_harmonics>
         ::call(corr, index, min_period, threshold);
      return found;
   }

   template <typename T>
   inline float pitch_detector<T>::calculate_frequency() const
   {
      auto h = harmonic();
      auto span = _bacf.get_span();
      if (!span.first|| !span.second)
         return -1.0f;

      assert(h == span.harmonic);

      // Get the start edge
      auto prev1 = span.first->_crossing.first;
      auto curr1 = span.first->_crossing.second;
      auto dy1 = curr1 - prev1;
      auto dx1 = -prev1 / dy1;

      // Get the next edge
      auto prev2 = span.second->_crossing.first;
      auto curr2 = span.second->_crossing.second;
      auto dy2 = curr2 - prev2;
      auto dx2 = -prev2 / dy2;

      // Calculate the frequency
      auto n_span = span.second->_leading_edge - span.first->_leading_edge;
      float n_samples = n_span + (dx2 - dx1);
      return (_sps * h) / n_samples;
   }

   template <typename T>
   float pitch_detector<T>::periodicity() const
   {
      if (_frequency == -1.0f)
         return 0.0f;
      auto const& info = _bacf.result();
      return 1.0 - (float(info.min_count) / info.max_count);
   }
}}

#endif

