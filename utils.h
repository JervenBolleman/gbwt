/*
  Copyright (c) 2015, 2016, 2017 Genome Research Ltd.

  Author: Jouni Siren <jouni.siren@iki.fi>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef GBWT_UTILS_H
#define GBWT_UTILS_H

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include <sdsl/bit_vectors.hpp>

#include <atomic>
#include <omp.h>

namespace gbwt
{

/*
  utils.h: Common utility methods.
*/

//------------------------------------------------------------------------------

/*
  We can save a lot of memory during construction by using 32-bit integers instead of
  64-bit integers in the dynamic records. This limits the number of nodes and the
  number of occurrences of each node to less than 2^32.
*/

#define GBWT_SAVE_MEMORY

//------------------------------------------------------------------------------

typedef std::uint64_t size_type;
typedef std::uint32_t short_type;
typedef std::uint8_t  byte_type;

typedef size_type node_type;
typedef size_type rank_type;  // Rank of incoming / outgoing edge.

#ifdef GBWT_SAVE_MEMORY
typedef std::pair<short_type, short_type> edge_type;
typedef std::pair<short_type, short_type> run_type;
typedef std::pair<short_type, short_type> sample_type;  // (i, DA[i]) within a record
#else
typedef std::pair<node_type, size_type>   edge_type;
typedef std::pair<rank_type, size_type>   run_type;
typedef std::pair<size_type, size_type>   sample_type;  // (i, DA[i]) within a record
#endif

//------------------------------------------------------------------------------

const size_type BYTE_BITS    = 8;
const size_type WORD_BITS    = 64;

const size_type KILOBYTE     = 1024;
const size_type MEGABYTE     = KILOBYTE * KILOBYTE;
const size_type GIGABYTE     = KILOBYTE * MEGABYTE;

const double KILOBYTE_DOUBLE = 1024.0;
const double MILLION_DOUBLE  = 1000000.0;
const double MEGABYTE_DOUBLE = KILOBYTE_DOUBLE * KILOBYTE_DOUBLE;
const double GIGABYTE_DOUBLE = KILOBYTE_DOUBLE * MEGABYTE_DOUBLE;

const size_type MILLION      = 1000000;
const size_type BILLION      = 1000 * MILLION;

const node_type ENDMARKER    = 0;

inline size_type invalid_sequence() { return ~(size_type)0; }
inline size_type invalid_offset() { return ~(size_type)0; }
inline edge_type invalid_edge() { return edge_type(ENDMARKER, invalid_offset()); }

//------------------------------------------------------------------------------

typedef sdsl::int_vector<0>        text_type;
typedef sdsl::int_vector_buffer<0> text_buffer_type;

//------------------------------------------------------------------------------

/*
  range_type stores a closed range [first, second]. Empty ranges are indicated by
  first > second. The emptiness check uses +1 to handle the common special case
  [0, -1].
*/

typedef std::pair<size_type, size_type> range_type;

struct Range
{
  inline static size_type length(range_type range)
  {
    return range.second + 1 - range.first;
  }

  inline static bool empty(range_type range)
  {
    return (range.first + 1 > range.second + 1);
  }

  inline static bool empty(size_type sp, size_type ep)
  {
    return (sp + 1 > ep + 1);
  }

  inline static size_type bound(size_type value, range_type bounds)
  {
    return bound(value, bounds.first, bounds.second);
  }

  inline static size_type bound(size_type value, size_type low, size_type high)
  {
    return std::max(std::min(value, high), low);
  }

  inline static range_type empty_range()
  {
    return range_type(1, 0);
  }

  /*
    Partition the range approximately evenly between the blocks. The actual number of
    blocks will not be greater than the length of the range.
  */
  static std::vector<range_type> partition(range_type range, size_type blocks);
};

template<class A, class B>
std::ostream& operator<<(std::ostream& out, const std::pair<A, B>& data)
{
  return out << "(" << data.first << ", " << data.second << ")";
}

template<class A>
std::ostream& operator<<(std::ostream& out, const std::vector<A>& data)
{
  out << "{ ";
  for(const A& element : data) { out << element << " "; }
  out << "}";
  return out;
}

//------------------------------------------------------------------------------

/*
  Global verbosity setting for index construction. Used in conditions of type
  if(Verbosity::level >= Verbosity::THRESHOLD). While the level can be set directly,
  Verbosity::set() does a few sanity checks.

  SILENT    no status information
  BASIC     basic statistics on the input and the final index
  EXTENDED  intermediate statistics for each batch
  FULL      further details of each batch
*/

struct Verbosity
{
  static size_type level;

  static void set(size_type new_level);
  static std::string levelName();

  const static size_type SILENT   = 0;
  const static size_type BASIC    = 1;
  const static size_type EXTENDED = 2;
  const static size_type DEFAULT  = 3;
  const static size_type FULL     = 3;
};

//------------------------------------------------------------------------------

template<class IntegerType>
inline size_type
bit_length(IntegerType val)
{
  return sdsl::bits::hi(val) + 1;
}

//------------------------------------------------------------------------------

const size_type FNV_OFFSET_BASIS = 0xcbf29ce484222325UL;
const size_type FNV_PRIME        = 0x100000001b3UL;

inline size_type fnv1a_hash(byte_type b, size_type seed)
{
  return (seed ^ b) * FNV_PRIME;
}

inline size_type fnv1a_hash(size_type val, size_type seed)
{
  byte_type* chars = (byte_type*)&val;
  for(size_type i = 0; i < 8; i++) { seed = fnv1a_hash(chars[i], seed); }
  return seed;
}

template<class ByteArray>
size_type fnv1a_hash(ByteArray& array)
{
  size_type res = FNV_OFFSET_BASIS;
  for(size_type i = 0; i < array.size(); i++) { res = fnv1a_hash((byte_type)(array[i]), res); }
  return res;
}

//------------------------------------------------------------------------------

inline double
inMegabytes(size_type bytes)
{
  return bytes / MEGABYTE_DOUBLE;
}

inline double
inGigabytes(size_type bytes)
{
  return bytes / GIGABYTE_DOUBLE;
}

inline double
inBPC(size_type bytes, size_type size)
{
  return (8.0 * bytes) / size;
}

inline double
inMicroseconds(double seconds)
{
  return seconds * MILLION_DOUBLE;
}

const size_type DEFAULT_INDENT = 18;

void printHeader(const std::string& header, size_type indent = DEFAULT_INDENT);
void printTime(const std::string& header, size_type queries, double seconds, size_type indent = DEFAULT_INDENT);

//------------------------------------------------------------------------------

double readTimer();       // Seconds from an arbitrary time point.
size_type memoryUsage();  // Peak memory usage in bytes.

//------------------------------------------------------------------------------

struct TempFile
{
  static std::atomic<size_type> counter;
  static std::string            temp_dir;
  const static std::string      DEFAULT_TEMP_DIR;

  static void setDirectory(const std::string& directory);
  static std::string getName(const std::string& name_part);
  static void remove(std::string& filename);
};

// Returns the total length of the rows, excluding line ends.
size_type readRows(const std::string& filename, std::vector<std::string>& rows, bool skip_empty_rows);

size_type fileSize(std::ifstream& file);
size_type fileSize(std::ofstream& file);

//------------------------------------------------------------------------------

/*
  parallelQuickSort() uses less working space than parallelMergeSort(). Calling omp_set_nested(1)
  improves the speed of parallelQuickSort().

  Sequential sorting is typically better with less than 1000 elements per thread.
*/

template<class Iterator, class Comparator>
void
parallelQuickSort(Iterator first, Iterator last, const Comparator& comp)
{
#ifdef _GLIBCXX_PARALLEL
  int nested = omp_get_nested();
  omp_set_nested(1);
  std::sort(first, last, comp, __gnu_parallel::balanced_quicksort_tag());
  omp_set_nested(nested);
#else
  std::sort(first, last, comp);
#endif
}

template<class Iterator>
void
parallelQuickSort(Iterator first, Iterator last)
{
#ifdef _GLIBCXX_PARALLEL
  int nested = omp_get_nested();
  omp_set_nested(1);
  std::sort(first, last, __gnu_parallel::balanced_quicksort_tag());
  omp_set_nested(nested);
#else
  std::sort(first, last);
#endif
}

template<class Iterator, class Comparator>
void
parallelMergeSort(Iterator first, Iterator last, const Comparator& comp)
{
#ifdef _GLIBCXX_PARALLEL
  std::sort(first, last, comp, __gnu_parallel::multiway_mergesort_tag());
#else
  std::sort(first, last, comp);
#endif
}

template<class Iterator>
void
parallelMergeSort(Iterator first, Iterator last)
{
#ifdef _GLIBCXX_PARALLEL
  std::sort(first, last, __gnu_parallel::multiway_mergesort_tag());
#else
  std::sort(first, last);
#endif
}

template<class Iterator, class Comparator>
void
sequentialSort(Iterator first, Iterator last, const Comparator& comp)
{
#ifdef _GLIBCXX_PARALLEL
  std::sort(first, last, comp, __gnu_parallel::sequential_tag());
#else
  std::sort(first, last, comp);
#endif
}

template<class Iterator>
void
sequentialSort(Iterator first, Iterator last)
{
#ifdef _GLIBCXX_PARALLEL
  std::sort(first, last, __gnu_parallel::sequential_tag());
#else
  std::sort(first, last);
#endif
}

const size_type PARALLEL_SORT_THRESHOLD = 1024;

template<class Iterator, class Comparator>
void
chooseBestSort(Iterator first, Iterator last, const Comparator& comp)
{
  size_type old_threads = omp_get_max_threads();
  size_type new_threads = ((last - first) + PARALLEL_SORT_THRESHOLD / 2) / PARALLEL_SORT_THRESHOLD;
  if(new_threads <= 1) { sequentialSort(first, last, comp); return; }

  if(new_threads < old_threads) { omp_set_num_threads(new_threads); }
  parallelQuickSort(first, last, comp);
  if(new_threads < old_threads) { omp_set_num_threads(old_threads); }
}

template<class Iterator>
void
chooseBestSort(Iterator first, Iterator last)
{
  size_type old_threads = omp_get_max_threads();
  size_type new_threads = ((last - first) + PARALLEL_SORT_THRESHOLD / 2) / PARALLEL_SORT_THRESHOLD;
  if(new_threads <= 1) { sequentialSort(first, last); return; }

  if(new_threads < old_threads) { omp_set_num_threads(new_threads); }
  parallelQuickSort(first, last);
  if(new_threads < old_threads) { omp_set_num_threads(old_threads); }
}

template<class Element>
void
removeDuplicates(std::vector<Element>& vec, bool parallel)
{
  if(parallel) { parallelQuickSort(vec.begin(), vec.end()); }
  else         { sequentialSort(vec.begin(), vec.end()); }
  vec.resize(std::unique(vec.begin(), vec.end()) - vec.begin());
}

//------------------------------------------------------------------------------

} // namespace gbwt

#endif // GBWT_UTILS_H
