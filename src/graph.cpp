#include "graph_util.hpp"

namespace GrappleMap {

void Graph::changed(PositionInSequence const pis)
{
	Edge & edge = edges.at(pis.sequence.index);

	if (ReorientedNode * const rn = node(pis))
	{
		Reoriented<NodeNum> const newn = find_or_add(edge.positions[pis.position.index]);

		NodeNum const oldn = **rn;

		*rn = newn;

		if (*newn != oldn)
		{
			std::cerr << "End of sequence is now a different node." << std::endl;

			compute_in_out(oldn);
			compute_in_out(*newn);
		}
	}
}

ReorientedNode * Graph::node(PositionInSequence const pis)
{
	if (pis.position.index == 0)
		return &edges[pis.sequence.index].from;
	if (!next(pis, *this))
		return &edges[pis.sequence.index].to;
	return nullptr;
}

void Graph::replace(PositionInSequence const pis, Position p, NodeModifyPolicy const policy)
{
	apply_limits(p);

	Edge & edge = edges.at(pis.sequence.index);
	Position & stored = edge.positions.at(pis.position.index);

	if (stored == p) return;

	if (ReorientedNode * const rn = node(pis))
	{
		if (auto reo = is_reoriented(nodes[(*rn)->index].position, p))
		{
//			std::cerr << "Recognized position as mere reorientation.\n";
			stored = p;
			rn->reorientation = *reo;
			assert(basicallySame((*this)[*rn], p));
		}
		else switch (policy)
		{
			case NodeModifyPolicy::propagate:
			{
	//			std::cerr << "Change to connecting position, updating connected edges.\n";
				stored = p;
				nodes[(*rn)->index].position = inverse(rn->reorientation)(p);
				assert(basicallySame((*this)[*rn], p));

				foreach (e : edges)
				{
					if (*e.from == **rn)
						e.positions.front() = (*this)[e.from];

					if (*e.to == **rn)
						e.positions.back() = (*this)[e.to];
				}
				break;
			}
			case NodeModifyPolicy::local:
			{
				stored = p;

				Reoriented<NodeNum> const newn = find_or_add(p);

				NodeNum const oldn = **rn;

				*rn = newn;

				if (*newn != oldn)
				{
					std::cerr << "End of sequence is now a different node." << std::endl;

					compute_in_out(oldn);
					compute_in_out(*newn);
				}

				break;
			}
			case NodeModifyPolicy::unintended:
			{
				assert(!"accidental attempt at non-reorienting edit of connecting position");
				break;
			}
		}
	}
	else stored = p;
}

void Graph::split_segment(Location const loc)
{
	auto & positions =  edges.at(loc.segment.sequence.index).positions;
	Position const p = at(loc, *this);
	positions.insert(positions.begin() + loc.segment.segment.index + 1, p);
}

void Graph::clone(PositionInSequence const pis) // todo: remove
{
	auto & positions =  edges.at(pis.sequence.index).positions;
	Position const p = positions.at(pis.position.index);
	positions.insert(positions.begin() + pis.position.index, p);
}

void Graph::insert_sequences(vector<Sequence> && v) // for bulk, more efficient than individually
{
	foreach (s : v)
	{
		ReorientedNode const
			from = find_or_add(s.positions.front()),
			to = find_or_add(s.positions.back());
		edges.push_back(Edge{from, to, std::move(s)});
	}

	foreach (n : nodenums(*this))
		compute_in_out(n);
}

void Graph::set(optional<SeqNum> const num, optional<Sequence> const seq)
{
	if (seq)
	{
		Edge e{
			find_or_add(seq->positions.front()),
			find_or_add(seq->positions.back()),
			*seq};

		if (num)
			edges[num->index] = e;
		else
			edges.push_back(e);

		compute_in_out(*e.from);
		compute_in_out(*e.to);
	}
	else if (num)
	{
		edges.erase(edges.begin() + num->index);

		foreach (n : nodenums(*this))
			compute_in_out(n);
	}
}

optional<PosNum> Graph::erase(PositionInSequence const pis)
{
	auto & edge = edges.at(pis.sequence.index);

	if (edge.positions.size() == 2)
	{
		std::cerr << "Cannot erase either of last two elements in sequence." << std::endl;
		return none;
	}

	edge.positions.erase(edge.positions.begin() + pis.position.index);

	auto const pos = std::min(pis.position, last_pos((*this)[pis.sequence]));

	changed({pis.sequence, pos});

	return pos;
}

optional<Reoriented<NodeNum>> Graph::is_reoriented_node(Position const & p) const
{
	foreach(n : nodenums(*this))
		if (auto r = is_reoriented(nodes[n.index].position, p))
			return n * *r;

	return none;
}

Reoriented<NodeNum> Graph::find_or_add(Position const & p)
{
	if (auto m = is_reoriented_node(p))
		return *m;

	nodes.push_back(Node{p, vector<string>(), {}, {}, {}, {}});

	NodeNum const nn{uint16_t(nodes.size() - 1)};

	compute_in_out(nn);

	return nn * PositionReorientation{};
}

void Graph::compute_in_out(NodeNum const n)
{
	Node & node = nodes[n.index];

	node.in.clear();
	node.out.clear();
	node.in_out.clear();

	for (SeqNum s{0}; s.index != edges.size(); ++s.index)
	{
		ReorientedNode const
			& from_ = (*this)[s].from,
			& to_ = (*this)[s].to;

		if (*to_ == n)
		{
			node.in.push_back({s, false});
			node.in_out.push_back({s, true});
		}

		if (*from_ == n)
		{
			node.out.push_back({s, false});
			node.in_out.push_back({s, false});
		}

		if (edges[s.index].bidirectional)
		{
			if (*from_ == n) node.in.push_back({s, true});
			if (*to_ == n) node.out.push_back({s, true});
		}
	}
}

}
