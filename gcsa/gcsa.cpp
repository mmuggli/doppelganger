#include <algorithm>
#include <fstream>
#include <iostream>
#include <stack>

#include <map>
#include <misc/utils.h>

#include "gcsa.h"


//TODO find every malloc(CHARS*CHARS) and add a corresponding free()
namespace GCSA
{
const usint CHARS = CSA::CHARS;
const usint MEGABYTE = CSA::MEGABYTE;
const pair_type EMPTY_PAIR = CSA::EMPTY_PAIR;
typedef CSA::uchar uchar;
typedef CSA::pair_type pair_type;
typedef CSA::sint sint;
//--------------------------------------------------------------------------

GCSA::GCSA(const std::string& base_name) :
  ok(false), support_locate(false),
  outgoing(0),
  sampled_positions(0), samples(0),
  alphabet(0),
  backbone(0)
{
  this->array = (CSA::BitVector**)malloc(CHARS*sizeof(CSA::DeltaVector *));
  for(usint i = 0; i < CHARS; i++) { this->array[i] = 0; }

  std::string index_name = base_name + GCSA_EXTENSION;
  std::ifstream input(index_name.c_str(), std::ios_base::binary);
  if(!input)
  {
    std::cerr << "Error opening input file (" << index_name << ")!" << std::endl;
    return;
  }

  this->alphabet = new CSA::Alphabet(input);
  for(usint i = 1; i < CHARS; i++)
  {
    if(this->alphabet->hasChar(i)) { this->array[i] = new CSA::DeltaVector(input); }
    else { this->array[i] = 0; }
  }
  this->outgoing = new CSA::RLEVector(input);
  this->node_count = this->outgoing->getNumberOfItems();

  this->sampled_positions = new CSA::DeltaVector(input);
  usint sample_bits = 0;
  input.read((char*)&sample_bits, sizeof(usint));
  if(sample_bits > 0)
  {
    this->samples = new CSA::ReadBuffer(input, this->sampled_positions->getNumberOfItems(), sample_bits);
    this->support_locate = true;
  }

  this->ok = true;
  input.close();
}

GCSA::GCSA(PathGraph& graph, Graph& parent, bool print) :
  ok(false), support_locate(false),
  outgoing(0),
  sampled_positions(0), samples(0),
  alphabet(0),
  backbone(0)
{
    if(graph.status != PathGraph::sorted || !(parent.ok)) { return; }
    this->array = (CSA::BitVector**)malloc(CHARS*sizeof(CSA::DeltaVector *));
//  DeltaEncoder* array_encoders[CHARS];
    std::map<usint, CSA::DeltaEncoder*> array_encoders;
    //DeltaEncoder** array_encoders = (DeltaEncoder**)malloc(CHARS);
    CSA::RLEEncoder outedges(OUTGOING_BLOCK_SIZE);
    usint *counts = (usint*)malloc(CHARS*sizeof(usint *));
//  usint counts[CHARS*CHARS];
    // std::cout << "initializing array encoders..." << std::endl;
    // for(usint i = 0; i < CHARS; i++)
    // {
    //     if (i % 1000 == 0) std::cout << i/1000 << " thousand initialized" << std::endl;
    //     this->array[i] = 0;
    //     counts[i] = 0;
    //     array_encoders[i] = new DeltaEncoder(ARRAY_BLOCK_SIZE); // FIXME this uses a lot of memory
    // }
    // std::cout << "done initializing array encoders" << std::endl;
    if(print) { std::cout << "Generating edges... "; std::cout.flush(); }
    if(!graph.generateEdges(parent)) { return; }
    if(print) { std::cout << graph.edge_count << " edges." << std::endl; }


    usint offset = 0, edge_offset = 0;
    std::cout << "Writing BWT and M..." << std::endl;
    for(std::vector<PathNode>::iterator node = graph.nodes.begin(); node != graph.nodes.end(); ++node)
    {
        // Write BWT.
        pair_type edge_range = graph.getEdges(node - graph.nodes.begin(), false);
        for(usint i = edge_range.first; i <= edge_range.second; i++)
        {
            uint label = graph.edges[i].label;
            if (label == 257) std::cout << "found larger label "<<label <<  std::endl;
            counts[label]++;
            if (array_encoders.find(label) == array_encoders.end()) {
                array_encoders[label] = new CSA::DeltaEncoder(ARRAY_BLOCK_SIZE); // FIXME this uses a lot of memory
            }
            array_encoders[label]->setBit(offset);
        }
        offset++;

        // Write M
        outedges.addBit(edge_offset);
        edge_offset += std::max((usint)1, (*node).outdegree());
    }
    std::cout << "Done.   array_encoders has " << array_encoders.size() << " elements." << std::endl;
    counts[0] = graph.automata;
    CSA::Alphabet *thealphabet = new CSA::Alphabet(counts);
    this->alphabet = thealphabet;

    for(usint i = 1; i < CHARS; i++)
    {

        if(this->alphabet->hasChar(i)) {
            if (array_encoders.find(i) == array_encoders.end()) {
                std::cout << "alphabet has " << i << " but array_encoders does not!" << std::endl;
                array_encoders[i] = new CSA::DeltaEncoder(ARRAY_BLOCK_SIZE); // FIXME this uses a lot of memory
            }
            this->array[i] = new CSA::DeltaVector(*(array_encoders[i]), offset);
        }
    }
    outedges.flush();
    this->outgoing = new CSA::RLEVector(outedges, edge_offset);
    this->node_count = this->outgoing->getNumberOfItems();


    // Create the backbone.
    if(parent.backbone != 0)
    {
        this->backbone = new Backbone(*this, graph, parent, print);
    }


    // Sample the graph for locate().
    if(print) { std::cout << "Sampling the graph... "; std::cout.flush();  }
    usint max_sample = 0;
    std::vector<pair_type>* sample_pairs = graph.getSamples(SAMPLE_RATE, max_sample, parent);
    CSA::DeltaEncoder sample_encoder(SAMPLE_BLOCK_SIZE);
    CSA::WriteBuffer sample_values(sample_pairs->size(), CSA::length(max_sample));
    CSA::parallelSort(sample_pairs->begin(), sample_pairs->end());
    for(usint i = 0; i < sample_pairs->size(); i++)
    {
        sample_encoder.addBit(sample_pairs->at(i).first);
        sample_values.writeItem(sample_pairs->at(i).second);
    }
    sample_encoder.flush();
    delete sample_pairs; sample_pairs = 0;

    this->sampled_positions = new CSA::DeltaVector(sample_encoder, offset);
    this->samples = sample_values.getReadBuffer();
    this->support_locate = true;
    if(print)
    {
        std::cout << this->sampled_positions->getNumberOfItems() << " samples." << std::endl;
        std::cout << std::endl;
    }


    if(print)
    {
        std::cout << "Nodes:           " << graph.node_count << std::endl;
        std::cout << "Edges:           " << graph.edge_count << std::endl;
        std::cout << "Automata:        " << graph.automata << std::endl;
        std::cout << "Samples:         " << this->sampled_positions->getNumberOfItems() << std::endl;
        std::cout << std::endl;

        this->reportSize(true);
    }
    this->ok = true;
    //free(array_encoders); //FIXME: switch to hash version
    free(counts);
}

GCSA::~GCSA()
{
  for(usint i = 0; i < CHARS; i++)
  {
    delete this->array[i]; this->array[i] = 0;
  }
  delete this->outgoing; this->outgoing = 0;
  delete this->sampled_positions; this->sampled_positions = 0;
  delete this->samples; this->samples = 0;
  delete this->alphabet; this->alphabet = 0;
  delete this->backbone; this->backbone = 0;
  free(array);
}

void
GCSA::writeTo(const std::string& base_name) const
{
  std::string index_name = base_name + GCSA_EXTENSION;
  std::ofstream output(index_name.c_str(), std::ios_base::binary);
  if(!output)
  {
    std::cerr << "Error opening output file (" << index_name << ")!" << std::endl;
    return;
  }

  this->alphabet->writeTo(output);
  for(usint i = 1; i < CHARS; i++)
  {
    if(this->alphabet->hasChar(i)) { this->array[i]->writeTo(output); }
  }
  this->outgoing->writeTo(output);

  this->sampled_positions->writeTo(output);
  usint sample_bits = (this->support_locate ? this->samples->getItemSize() : 0);
  output.write((char*)&sample_bits, sizeof(usint));
  if(this->support_locate) { this->samples->writeBuffer(output); }

  output.close();

  if(this->backbone != 0)
  {
    this->backbone->writeTo(base_name);
  }
}

bool
GCSA::isOk() const
{
  return this->ok;
}

//--------------------------------------------------------------------------

usint
GCSA::reportSize(bool print) const
{
  usint array_size = 0;
  for(usint i = 1; i < CHARS; i++)
  {
    if(this->alphabet->hasChar(i)) { array_size += this->array[i]->reportSize(); }
  }

  usint outedges = this->outgoing->reportSize();

  usint sa_samples = this->sampled_positions->reportSize();
  if(this->support_locate) { sa_samples += this->samples->reportSize(); }

  usint bytes = sizeof(*this) + array_size + outedges + sa_samples + this->alphabet->reportSize();
  if(print)
  {
    std::cout << "Array:           " << (array_size / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Outgoing edges:  " << (outedges / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Samples:         " << (sa_samples / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Total size:      " << (bytes / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << std::endl;
  }

  if(this->backbone != 0)
  {
    if(print) { std::cout << "Backbone information:" << std::endl; }
    bytes += this->backbone->reportSize(print);
  }

  return bytes;
}

void
GCSA::printCounts() const
{
  for(usint i = 0; i < CHARS; i++)
  {
    if(this->alphabet->hasChar(i))
    {
      std::cout << "counts[" << i << " (" << ((uchar)i) << ")] = " << this->alphabet->countOf(i) << std::endl;
    }
  }
  std::cout << std::endl;
}

//--------------------------------------------------------------------------

pair_type
GCSA::find(const std::string& pattern) const
{
  if(pattern.length() == 0) { return this->getSARange(); }

  sint pos = pattern.length() - 1;
  pair_type range = this->getCharRange(pattern[pos]);
  if(CSA::isEmpty(range)) { return range; }

  for(pos--; pos >= 0; pos--)
  {
    range = this->LF(range, pattern[pos]);
    if(CSA::isEmpty(range)) { return range; }
  }
  return range;
}

//--------------------------------------------------------------------------

std::vector<usint>*
GCSA::locate(pair_type range) const
{
  if(!(this->support_locate)) { return 0; }
  std::vector<usint>* vec = new std::vector<usint>;

  this->locate(range, *vec);
  CSA::removeDuplicates(vec, false);

  return vec;
}

std::vector<usint>*
GCSA::locate(std::vector<pair_type>& ranges) const
{
  if(!(this->support_locate)) { return 0; }
  std::vector<usint>* vec = new std::vector<usint>;

  for(std::vector<pair_type>::iterator iter = ranges.begin(); iter != ranges.end(); ++iter)
  {
    this->locate(*iter, *vec);
  }
  CSA::removeDuplicates(vec, false);

  return vec;
}

bool
GCSA::isSampled(usint index) const
{
  if(!(this->support_locate) || index >= this->getSize())  { return false; }

  CSA::DeltaVector::Iterator sample_iter(*(this->sampled_positions));
  return sample_iter.isSet(index);
}

usint
GCSA::locate(usint index) const
{
  if(!(this->support_locate) || index >= this->getSize())  { return this->getSize(); }

  return this->locateUnsafe(index);
}

usint
GCSA::labelOf(usint index) const
{
  if(index >= this->getSize()) { return 0; }

  CSA::BitVector::Iterator* outgoing_iter = this->outgoing->newIterator();
  index = outgoing_iter->select(index);
  delete outgoing_iter;
  return this->alphabet->charAt(index);
}

//--------------------------------------------------------------------------

pair_type
GCSA::getSARange() const
{
  return pair_type(0, this->getSize() - 1);
}

pair_type
GCSA::getBWTRange() const
{
  return pair_type(0, this->getSize() - 1);
}

pair_type
GCSA::getCharRange(usint c) const
{
  pair_type range = this->alphabet->getRange(c);
  if(CSA::isEmpty(range)) { return range; }
  return this->convertToNodeRange(range);
}

void
GCSA::convertToBWTRange(pair_type& sa_range) const
{
}

void
GCSA::convertToSARange(pair_type& bwt_range) const
{
}

void
GCSA::convertToSARange(std::vector<pair_type>& bwt_ranges) const
{
}

pair_type
GCSA::LF(pair_type range, usint c) const
{
  if(!(this->alphabet->hasChar(c))) { return EMPTY_PAIR; }

  // Follow edges backward using BWT.
  CSA::BitVector::Iterator* array_iter = this->array[c]->newIterator();
  range.first = this->alphabet->cumulative(c) + array_iter->rank(range.first, true) - 1;
  range.second = this->alphabet->cumulative(c) + array_iter->rank(range.second) - 1;
  if(CSA::isEmpty(range)) { return EMPTY_PAIR; }
  delete array_iter;

  return this->convertToNodeRange(range);
}

std::vector<usint>*
GCSA::locateRange(pair_type range) const
{
  return this->locate(range);
}

std::vector<usint>*
GCSA::locateRanges(std::vector<pair_type>& ranges) const
{
  return this->locate(ranges);
}

//--------------------------------------------------------------------------

std::vector<usint>*
GCSA::getSuccessors(usint index) const
{
  if(index >= this->getSize()) { return 0; }

  std::vector<usint>* result = new std::vector<usint>;
  if(index == 0) { return result; } // Final node.

  CSA::BitVector::Iterator* outgoing_iter = this->outgoing->newIterator();
  index = outgoing_iter->select(index);
  usint successors = outgoing_iter->selectNext() - index;
  usint c = this->alphabet->charAt(index);

  // Find the corresponding incoming edges using BWT.
  CSA::BitVector::Iterator* array_iter = this->array[c]->newIterator();
  result->push_back(array_iter->select(index - this->alphabet->cumulative(c)));
  for(usint i = 1; i < successors; i++) { result->push_back(array_iter->selectNext()); }
  delete outgoing_iter;
  delete array_iter;
  return result;
}

CSA::BitVector::Iterator*
GCSA::getIterator(usint c) const
{
  if(c < CHARS && c > 0 && this->alphabet->hasChar(c)) { return this->array[c]->newIterator(); }
  return 0;
}

CSA::BitVector::Iterator*
GCSA::getEdgeIterator() const
{
    return this->outgoing->newIterator();
}

//--------------------------------------------------------------------------

pair_type
GCSA::convertToNodeRange(pair_type edge_range) const
{
  CSA::BitVector::Iterator* outgoing_iter = this->outgoing->newIterator();
  edge_range.first = outgoing_iter->rank(edge_range.first) - 1;
  edge_range.second = outgoing_iter->rank(edge_range.second) - 1;
  delete outgoing_iter;
  return edge_range;
}

usint
GCSA::Psi(usint index) const
{
  if(index == 0) { return this->getSize() - 1; } // Final node.

  CSA::BitVector::Iterator* outgoing_iter = this->outgoing->newIterator();
  index = outgoing_iter->select(index);
  usint c = this->alphabet->charAt(index);

  // Find the corresponding incoming edge using BWT.
  CSA::BitVector::Iterator* array_iter = this->array[c]->newIterator();
  index = array_iter->select(index - this->alphabet->cumulative(c));
  delete outgoing_iter;
  delete array_iter;
  return index;
}

usint
GCSA::LF(usint index, usint c) const
{
	CSA::BitVector::Iterator* array_iter = this->array[c]->newIterator();
  index = this->alphabet->cumulative(c) + array_iter->rank(index) - 1;
  delete array_iter;
  CSA::BitVector::Iterator* edge_iter = this->outgoing->newIterator();
  index = edge_iter->rank(index) - 1;

  return index;
}

//--------------------------------------------------------------------------

void
GCSA::locate(pair_type range, std::vector<usint>& vec) const
{
  range.second = std::min(range.second, this->getSize() - 1);
  if(CSA::isEmpty(range)) { return; }

  for(usint pos = range.first; pos <= range.second; pos++)
  {
    vec.push_back(this->locateUnsafe(pos));
  }
}

usint
GCSA::locateUnsafe(usint index) const
{
	CSA::DeltaVector::Iterator sample_iter(*(this->sampled_positions));
  usint temp = index, steps = 0;

  while(!(sample_iter.isSet(temp))) { temp = this->Psi(temp); steps++; }
  return this->samples->readItem(sample_iter.rank(temp) - 1) - steps;
}

//--------------------------------------------------------------------------


Backbone::Backbone(const GCSA& _gcsa, PathGraph& graph, Graph& parent, bool print) :
  gcsa(_gcsa),
  nodes(0), edges(0), original(0),
  size(0), ok(false)
{
  if(graph.status != PathGraph::ready || parent.backbone == 0) { return; }

  if(print)
  {
    std::cout << "Compressing backbone... "; std::cout.flush();
  }

  CSA::SuccinctEncoder original_encoder(NODE_BLOCK_SIZE);
  CSA::SuccinctVector::Iterator iter(*(parent.backbone));
  for(usint i = 0; i < graph.node_count; i++)
  {
    if(iter.isSet(graph.nodes[i].from)) { original_encoder.addBit(i); }
  }
  original_encoder.flush();
  this->original = new CSA::SuccinctVector(original_encoder, graph.node_count);
  if(print)
  {
    std::cout << "original(" << this->original->getNumberOfItems() << ") ";
    std::cout.flush();
  }


  // Use a kind of backward searching to find the backbone in PathGraph.
  usint bb_nodes = 0;
  for(usint j = 0; j < this->gcsa.getNumberOfAutomata(); j++)
  {
    usint curr = j;  // Final node of automaton j.
    graph.nodes[curr].setBackbone(); bb_nodes++;
    while(curr < this->gcsa.getSize() - this->gcsa.getNumberOfAutomata()) // Not the initial node.
    {
      bool found = false;
      pair_type edge_range = graph.getEdges(curr, false);
      for(usint i = edge_range.first; i <= edge_range.second; i++)
      {
        usint prev = graph.edges[i].from;
        if(iter.isSet(graph.nodes[prev].from) &&
           graph.nodes[prev].value() == graph.nodes[curr].value() - 1)
       {
          graph.nodes[prev].setBackbone(); bb_nodes++;
          curr = prev;
          found = true; break;
        }
      }
      if(!found)
      {
        std::cerr << "Error: Cannot find previous backbone node!" << std::endl;
        return;
      }
    }
  }
  if(print)
  {
    std::cout << "found(" << bb_nodes << ") "; std::cout.flush();
  }


  // Scan the graph forward and build backbone information.
  CSA::SuccinctEncoder node_encoder(NODE_BLOCK_SIZE);
  CSA::RLEEncoder edge_encoder(EDGE_BLOCK_SIZE);
  CSA::BitVector::Iterator* edge_iter = this->gcsa.outgoing->newIterator();
  usint offset = edge_iter->select(0);

  graph.sortEdges(true, true);
  for(usint i = 0; i < graph.node_count; i++)
  {
    if(graph.nodes[i].isBackbone())
    {
      node_encoder.addBit(i); this->size++;
      if(i >= this->gcsa.getNumberOfAutomata())
      {
        bool found = false;
        pair_type successors = graph.getEdges(i, true);
        for(usint j = successors.first; j <= successors.second; j++)
        {
          usint to = graph.edges[j].rank;
          if(graph.nodes[to].isBackbone() && graph.nodes[to].value() == graph.nodes[i].value() + 1)
          {
            offset += j - successors.first;
            found = true; break;
          }
        }
        if(!found)
        {
          std::cerr << "Error: Cannot find next backbone node!" << std::endl;
          std::cerr << "       i = " << i << ", from = " << graph.nodes[i].from << ", value = "
                    << graph.nodes[i].value() << std::endl;
          return;
        }
      }
    }
    edge_encoder.addBit(offset);
    offset = edge_iter->selectNext();
  }

  node_encoder.flush(); edge_encoder.flush();
  this->nodes = new CSA::SuccinctVector(node_encoder, graph.node_count);
  this->edges = new CSA::RLEVector(edge_encoder, this->gcsa.outgoing->getSize());


  if(print)
  {
    std::cout << "done." << std::endl;
  }
  this->ok = true;
  delete edge_iter;
}

Backbone::Backbone(const std::string& base_name, const GCSA& _gcsa) :
  gcsa(_gcsa),
  nodes(0), edges(0), original(0),
  ok(false)
{
  std::string backbone_name = base_name + BACKBONE_EXTENSION;
  std::ifstream input(backbone_name.c_str(), std::ios_base::binary);
  if(!input)
  {
    std::cerr << "Error opening input file (" << backbone_name << ")!" << std::endl;
    return;
  }

  input.read((char*)&(this->size), sizeof(this->size));
  this->nodes = new CSA::SuccinctVector(input);
  this->edges = new CSA::RLEVector(input);
  this->original = new CSA::SuccinctVector(input);

  this->ok = true;
  input.close();
}

Backbone::~Backbone()
{
  delete this->nodes; this->nodes = 0;
  delete this->edges; this->edges = 0;
  delete this->original; this->original = 0;
}

void
Backbone::writeTo(const std::string& base_name) const
{
  std::string backbone_name = base_name + BACKBONE_EXTENSION;
  std::ofstream output(backbone_name.c_str(), std::ios_base::binary);
  if(!output)
  {
    std::cerr << "Error opening output file (" << backbone_name << ")!" << std::endl;
    return;
  }

  output.write((char*)&(this->size), sizeof(this->size));
  this->nodes->writeTo(output);
  this->edges->writeTo(output);
  this->original->writeTo(output);

  output.close();
}

usint
Backbone::getSize() const
{
  return this->size;
}

bool
Backbone::isOk() const
{
  return this->ok;
}

usint
Backbone::reportSize(bool print) const
{
  usint nodes_size = this->nodes->reportSize();
  usint edges_size = this->edges->reportSize();
  usint original_size = this->original->reportSize();

  usint bytes = sizeof(*this) + nodes_size + edges_size + original_size;
  if(print)
  {
    std::cout << "Nodes:           " << (nodes_size / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Edges:           " << (edges_size / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Original:        " << (original_size / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << "Total size:      " << (bytes / (double)MEGABYTE) << " MB" << std::endl;
    std::cout << std::endl;
  }

  return bytes;
}

//--------------------------------------------------------------------------

bool
Backbone::contains(usint index) const
{
	CSA::SuccinctVector::Iterator iter(*(this->nodes));
  return iter.isSet(index);
}

bool
Backbone::originalContains(usint index) const
{
	CSA::SuccinctVector::Iterator iter(*(this->original));
  return iter.isSet(index);
}

usint
Backbone::next(usint index) const
{
  if(index < this->gcsa.getNumberOfAutomata())  // From final node to initial node.
  {
    return this->gcsa.getSize() - this->gcsa.getNumberOfAutomata() + index;
  }

  CSA::RLEVector::Iterator iter(*(this->edges));
  index = iter.select(index);
  usint c = this->gcsa.alphabet->charAt(index);

  // Find the corresponding incoming edge using BWT.
  CSA::BitVector::Iterator* array_iter = this->gcsa.array[c]->newIterator();
  index = array_iter->select(index - this->gcsa.alphabet->cumulative(c));
  delete array_iter;
  return index;
}

usint
Backbone::previous(usint index) const
{
  if(index >= this->gcsa.getSize() - this->gcsa.getNumberOfAutomata())  // From initial node to final node.
  {
    return index - (this->gcsa.getSize() - this->gcsa.getNumberOfAutomata());
  }

  CSA::Alphabet* alpha = this->gcsa.alphabet;
  for(usint i = 0; i < alpha->getAlphabetSize(); i++)
  {
    usint c = alpha->getTextChar(i);

    // If BWT[index] contains c, follow the corresponding edge backward.
    CSA::BitVector::Iterator* array_iter = this->gcsa.array[c]->newIterator();
    if(!(array_iter->isSet(index))) { continue; }
    index = array_iter->rank(index) - 1;

    // If we followed a backbone edge, return the node we ended up in.
    CSA::RLEVector::Iterator edge_iter(*(this->edges));
    if(!(edge_iter.isSet(index))) { continue; }
    delete array_iter;
    return edge_iter.rank(index) - 1;
  }

  return 0;
}

//--------------------------------------------------------------------------

} // namespace GCSA
