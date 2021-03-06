/*
  Copyright (c) 2017 Genome Research Ltd.

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

#include "internal.h"

namespace gbwt
{

//------------------------------------------------------------------------------

void
DynamicRecord::clear()
{
  DynamicRecord temp;
  this->swap(temp);
}

void
DynamicRecord::swap(DynamicRecord& another)
{
  if(this != &another)
  {
    std::swap(this->body_size, another.body_size);
    this->incoming.swap(another.incoming);
    this->outgoing.swap(another.outgoing);
    this->body.swap(another.body);
    this->ids.swap(another.ids);
  }
}

//------------------------------------------------------------------------------

void
DynamicRecord::recode()
{
  if(this->empty()) { return; }

  bool sorted = true;
  for(rank_type outrank = 1; outrank < this->outdegree(); outrank++)
  {
    if(this->successor(outrank) < this->successor(outrank - 1)) { sorted = false; break; }
  }
  if(sorted) { return; }

  for(run_type& run : this->body) { run.first = this->successor(run.first); }
  sequentialSort(this->outgoing.begin(), this->outgoing.end());
  for(run_type& run : this->body) { run.first = this->edgeTo(run.first); }
}

//------------------------------------------------------------------------------

edge_type
DynamicRecord::LF(size_type i) const
{
  if(i >= this->size()) { return invalid_edge(); }

  std::vector<edge_type> result(this->outgoing);
  rank_type last_edge = 0;
  size_type offset = 0;
  for(run_type run : this->body)
  {
    last_edge = run.first;
    result[run.first].second += run.second;
    offset += run.second;
    if(offset > i) { break; }
  }

  result[last_edge].second -= (offset - i);
  return result[last_edge];
}

size_type
DynamicRecord::LF(size_type i, node_type to) const
{
  size_type outrank = this->edgeTo(to);
  if(outrank >= this->outdegree()) { return invalid_offset(); }

  size_type result = this->offset(outrank), offset = 0;
  for(run_type run : this->body)
  {
    if(run.first == outrank) { result += run.second; }
    offset += run.second;
    if(offset >= i)
    {
      if(run.first == outrank) { result -= offset - i; }
      break;
    }
  }
  return result;
}

range_type
DynamicRecord::LF(range_type range, node_type to) const
{
  if(Range::empty(range)) { return Range::empty_range(); }

  size_type outrank = this->edgeTo(to);
  if(outrank >= this->outdegree()) { return Range::empty_range(); }

  std::vector<run_type>::const_iterator iter = this->body.begin();
  run_type run = *iter;
  size_type result = this->offset(outrank) + (run.first == outrank ? run.second : 0), offset = run.second;

  while(iter != body.end() && offset < range.first)
  {
    ++iter;
    if(iter == body.end()) { break; }
    run = *iter;
    if(run.first == outrank) { result += run.second; }
    offset += run.second;
  }
  range.first = result - (run.first == outrank ? offset - range.first : 0);

  while(iter != body.end() && offset < range.second)
  {
    ++iter;
    if(iter == body.end()) { break; }
    run = *iter;
    if(run.first == outrank) { result += run.second; }
    offset += run.second;
  }
  range.second = result - (run.first == outrank ? offset - range.second : 0);

  return range;
}

node_type
DynamicRecord::operator[](size_type i) const
{
  if(i >= this->size()) { return ENDMARKER; }

  size_type offset = 0;
  for(run_type run : this->body)
  {
    offset += run.second;
    if(offset > i) { return this->successor(run.first); }
  }

  return ENDMARKER;
}

//------------------------------------------------------------------------------

rank_type
DynamicRecord::edgeTo(node_type to) const
{
  for(rank_type outrank = 0; outrank < this->outdegree(); outrank++)
  {
    if(this->successor(outrank) == to) { return outrank; }
  }
  return this->outdegree();
}

//------------------------------------------------------------------------------

rank_type
DynamicRecord::findFirst(node_type from) const
{
  for(size_type i = 0; i < this->indegree(); i++)
  {
    if(this->incoming[i].first >= from) { return i; }
  }
  return this->indegree();
}

void
DynamicRecord::increment(node_type from)
{
  for(rank_type inrank = 0; inrank < this->indegree(); inrank++)
  {
    if(this->predecessor(inrank) == from) { this->count(inrank)++; return; }
  }
  this->addIncoming(edge_type(from, 1));
}

void
DynamicRecord::addIncoming(edge_type inedge)
{
  this->incoming.push_back(inedge);
  sequentialSort(this->incoming.begin(), this->incoming.end());
}

//------------------------------------------------------------------------------

std::ostream&
operator<<(std::ostream& out, const DynamicRecord& record)
{
  out << "(size " << record.size() << ", "
      << record.runs() << " runs, "
      << "indegree " << record.indegree()
      << ", outdegree " << record.outdegree()
      << ", incoming = " << record.incoming
      << ", outgoing = " << record.outgoing
      << ", body = " << record.body
      << ", ids = " << record.ids << ")";

  return out;
}

//------------------------------------------------------------------------------

CompressedRecord::CompressedRecord(const std::vector<byte_type>& source, size_type start, size_type limit)
{
  this->outgoing.resize(ByteCode::read(source, start));
  node_type prev = 0;
  for(edge_type& outedge : this->outgoing)
  {
    outedge.first = ByteCode::read(source, start) + prev;
    prev = outedge.first;
    outedge.second = ByteCode::read(source, start);
  }

  this->body = source.data() + start;
  this->data_size = limit - start;
}

size_type
CompressedRecord::size() const
{
  size_type result = 0;
  if(this->outdegree() > 0)
  {
    for(CompressedRecordIterator iter(*this); !(iter.end()); ++iter) { result += iter->second; }
  }
  return result;
}

size_type
CompressedRecord::runs() const
{
  size_type result = 0;
  if(this->outdegree() > 0)
  {
    for(CompressedRecordIterator iter(*this); !(iter.end()); ++iter) { result++; }
  }
  return result;
}

edge_type
CompressedRecord::LF(size_type i) const
{
  if(this->outdegree() == 0) { return invalid_edge(); }

  for(CompressedRecordFullIterator iter(*this); !(iter.end()); ++iter)
  {
    if(iter.offset() > i)
    {
      edge_type result = iter.edge(); result.second -= (iter.offset() - i);
      return result;
    }
  }
  return invalid_edge();
}

size_type
CompressedRecord::LF(size_type i, node_type to) const
{
  size_type outrank = this->edgeTo(to);
  if(outrank >= this->outdegree()) { return invalid_offset(); }
  CompressedRecordRankIterator iter(*this, outrank);

  while(!(iter.end()) && iter.offset() < i) { ++iter; }
  return iter.rankAt(i);
}

range_type
CompressedRecord::LF(range_type range, node_type to) const
{
  if(Range::empty(range)) { return Range::empty_range(); }

  size_type outrank = this->edgeTo(to);
  if(outrank >= this->outdegree()) { return Range::empty_range(); }
  CompressedRecordRankIterator iter(*this, outrank);

  while(!(iter.end()) && iter.offset() < range.first) { ++iter; }
  range.first = iter.rankAt(range.first);
  while(!(iter.end()) && iter.offset() < range.second) { ++iter; }
  range.second = iter.rankAt(range.second);

  return range;
}

node_type
CompressedRecord::operator[](size_type i) const
{
  if(this->outdegree() == 0) { return ENDMARKER; }

  for(CompressedRecordIterator iter(*this); !(iter.end()); ++iter)
  {
    if(iter.offset() > i) { return this->successor(iter->first); }
  }
  return ENDMARKER;
}

rank_type
CompressedRecord::edgeTo(node_type to) const
{
  for(rank_type outrank = 0; outrank < this->outdegree(); outrank++)
  {
    if(this->successor(outrank) == to) { return outrank; }
  }
  return this->outdegree();
}

//------------------------------------------------------------------------------

RecordArray::RecordArray() :
  records(0)
{
}

RecordArray::RecordArray(const RecordArray& source)
{
  this->copy(source);
}

RecordArray::RecordArray(RecordArray&& source)
{
  *this = std::move(source);
}

RecordArray::~RecordArray()
{
}

RecordArray::RecordArray(const std::vector<DynamicRecord>& bwt) :
  records(bwt.size())
{
  // Find the starting offsets and compress the BWT.
  std::vector<size_type> offsets(bwt.size());
  for(size_type i = 0; i < bwt.size(); i++)
  {
    offsets[i] = this->data.size();
    const DynamicRecord& current = bwt[i];

    // Write the outgoing edges.
    ByteCode::write(this->data, current.outdegree());
    node_type prev = 0;
    for(edge_type outedge : current.outgoing)
    {
      ByteCode::write(this->data, outedge.first - prev);
      prev = outedge.first;
      ByteCode::write(this->data, outedge.second);
    }

    // Write the body.
    if(current.outdegree() > 0)
    {
      Run encoder(current.outdegree());
      for(run_type run : current.body) { encoder.write(this->data, run); }
    }
  }

  // Compress the index.
  sdsl::sd_vector_builder builder(this->data.size(), offsets.size());
  for(size_type offset : offsets) { builder.set(offset); }
  this->index = sdsl::sd_vector<>(builder);
  sdsl::util::init_support(this->select, &(this->index));
}

void
RecordArray::swap(RecordArray& another)
{
  if(this != &another)
  {
    std::swap(this->records, another.records),
    this->index.swap(another.index);
    sdsl::util::swap_support(this->select, another.select, &(this->index), &(another.index));
    this->data.swap(another.data);
  }
}

RecordArray&
RecordArray::operator=(const RecordArray& source)
{
  if(this != &source) { this->copy(source); }
  return *this;
}

RecordArray&
RecordArray::operator=(RecordArray&& source)
{
  if(this != &source)
  {
    this->records = std::move(source.records);
    this->index = std::move(source.index);
    this->select = std::move(source.select); this->select.set_vector(&(this->index));
    this->data = std::move(source.data);
  }
  return *this;
}

size_type
RecordArray::serialize(std::ostream& out, sdsl::structure_tree_node* v, std::string name) const
{
  sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
  size_type written_bytes = 0;

  written_bytes += sdsl::write_member(this->records, out, child, "records");
  written_bytes += this->index.serialize(out, child, "index");
  written_bytes += this->select.serialize(out, child, "select");

  // Serialize the data.
  size_type data_bytes = this->data.size() * sizeof(byte_type);
  sdsl::structure_tree_node* data_node =
    sdsl::structure_tree::add_child(child, "data", "std::vector<gbwt::byte_type>");
  out.write((const char*)(this->data.data()), data_bytes);
  sdsl::structure_tree::add_size(data_node, data_bytes);
  written_bytes += data_bytes;

  sdsl::structure_tree::add_size(child, written_bytes);
  return written_bytes;
}

void
RecordArray::load(std::istream& in)
{
  sdsl::read_member(this->records, in);

  // Read the record index.
  this->index.load(in);
  this->select.load(in, &(this->index));

  // Read the data.
  this->data.resize(this->index.size());
  in.read((char*)(this->data.data()), this->data.size() * sizeof(byte_type));
}

void
RecordArray::copy(const RecordArray& source)
{
  this->records = source.records;
  this->index = source.index;
  this->select = source.select; this->select.set_vector(&(this->index));
  this->data = source.data;
}

//------------------------------------------------------------------------------

DASamples::DASamples()
{
}

DASamples::DASamples(const DASamples& source)
{
  this->copy(source);
}

DASamples::DASamples(DASamples&& source)
{
  *this = std::move(source);
}

DASamples::~DASamples()
{
}

DASamples::DASamples(const std::vector<DynamicRecord>& bwt)
{
  // Determine the statistics and mark the sampled nodes.
  size_type records = 0, offsets = 0, sample_count = 0;
  this->sampled_records = sdsl::bit_vector(bwt.size(), 0);
  for(size_type i = 0; i < bwt.size(); i++)
  {
    if(bwt[i].samples() > 0)
    {
      records++; offsets += bwt[i].size(); sample_count += bwt[i].samples();
      this->sampled_records[i] = 1;
    }
  }
  sdsl::util::init_support(this->record_rank, &(this->sampled_records));

  // Build the bitvectors over BWT offsets.
  sdsl::sd_vector_builder range_builder(offsets, records);
  sdsl::sd_vector_builder offset_builder(offsets, sample_count);
  size_type offset = 0, max_sample = 0;
  for(const DynamicRecord& record : bwt)
  {
    if(record.samples() > 0)
    {
      range_builder.set(offset);
      for(sample_type sample : record.ids)
      {
        offset_builder.set(offset + sample.first);
        max_sample = std::max(max_sample, (size_type)(sample.second));
      }
      offset += record.size();
    }
  }
  this->bwt_ranges = sdsl::sd_vector<>(range_builder);
  sdsl::util::init_support(this->bwt_select, &(this->bwt_ranges));
  this->sampled_offsets = sdsl::sd_vector<>(offset_builder);
  sdsl::util::init_support(this->sample_rank, &(this->sampled_offsets));

  // Store the samples.
  this->array = sdsl::int_vector<0>(sample_count, 0, bit_length(max_sample));
  size_type curr = 0;
  for(const DynamicRecord& record : bwt)
  {
    if(record.samples() > 0)
    {
      for(sample_type sample : record.ids) { this->array[curr] = sample.second; curr++; }
    }
  }
}

void
DASamples::swap(DASamples& another)
{
  if(this != &another)
  {
    this->sampled_records.swap(another.sampled_records);
    sdsl::util::swap_support(this->record_rank, another.record_rank, &(this->sampled_records), &(another.sampled_records));

    this->bwt_ranges.swap(another.bwt_ranges);
    sdsl::util::swap_support(this->bwt_select, another.bwt_select, &(this->bwt_ranges), &(another.bwt_ranges));

    this->sampled_offsets.swap(another.sampled_offsets);
    sdsl::util::swap_support(this->sample_rank, another.sample_rank, &(this->sampled_offsets), &(another.sampled_offsets));

    this->array.swap(another.array);
  }
}

DASamples&
DASamples::operator=(const DASamples& source)
{
  if(this != &source) { this->copy(source); }
  return *this;
}

DASamples&
DASamples::operator=(DASamples&& source)
{
  if(this != &source)
  {
    this->sampled_records = std::move(source.sampled_records);
    this->record_rank = std::move(source.record_rank);

    this->bwt_ranges = std::move(source.bwt_ranges);
    this->bwt_select = std::move(source.bwt_select);

    this->sampled_offsets = std::move(source.sampled_offsets);
    this->sample_rank = std::move(source.sample_rank);

    this->array = std::move(source.array);

    this->setVectors();
  }
  return *this;
}

size_type
DASamples::serialize(std::ostream& out, sdsl::structure_tree_node* v, std::string name) const
{
  sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
  size_type written_bytes = 0;

  written_bytes += this->sampled_records.serialize(out, child, "sampled_records");
  written_bytes += this->record_rank.serialize(out, child, "record_rank");

  written_bytes += this->bwt_ranges.serialize(out, child, "bwt_ranges");
  written_bytes += this->bwt_select.serialize(out, child, "bwt_select");

  written_bytes += this->sampled_offsets.serialize(out, child, "sampled_offsets");
  written_bytes += this->sample_rank.serialize(out, child, "sample_rank");

  written_bytes += this->array.serialize(out, child, "array");

  sdsl::structure_tree::add_size(child, written_bytes);
  return written_bytes;
}

void
DASamples::load(std::istream& in)
{
  this->sampled_records.load(in);
  this->record_rank.load(in, &(this->sampled_records));

  this->bwt_ranges.load(in);
  this->bwt_select.load(in, &(this->bwt_ranges));

  this->sampled_offsets.load(in);
  this->sample_rank.load(in, &(this->sampled_offsets));

  this->array.load(in);
}

void
DASamples::copy(const DASamples& source)
{
  this->sampled_records = source.sampled_records;
  this->record_rank = source.record_rank;

  this->bwt_ranges = source.bwt_ranges;
  this->bwt_select = source.bwt_select;

  this->sampled_offsets = source.sampled_offsets;
  this->sample_rank = source.sample_rank;

  this->array = source.array;

  this->setVectors();
}

void
DASamples::setVectors()
{
  this->record_rank.set_vector(&(this->sampled_records));
  this->bwt_select.set_vector(&(this->bwt_ranges));
  this->sample_rank.set_vector(&(this->sampled_offsets));
}

size_type
DASamples::tryLocate(size_type record, size_type offset) const
{
  if(this->sampled_records[record] == 0) { return invalid_sequence(); }

  size_type record_start = this->bwt_select(this->record_rank(record) + 1);
  if(this->sampled_offsets[record_start + offset])
  {
    return this->array[this->sample_rank(record_start + offset)];
  }
  return invalid_sequence();
}

//------------------------------------------------------------------------------

} // namespace gbwt
