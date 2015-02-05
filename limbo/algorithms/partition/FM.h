/*************************************************************************
    > File Name: FM.h
    > Author: Yibo Lin
    > Mail: yibolin@utexas.edu
    > Created Time: Tue 03 Feb 2015 03:56:27 PM CST
 ************************************************************************/

#ifndef _LIMBO_ALGORITHMS_PARTITION_FM_H
#define _LIMBO_ALGORITHMS_PARTITION_FM_H

/// ===================================================================
///    class          : FM
///
///  Refer to Fiduccia and Mattheyses,
///  "A Linear-time Heuristics for Improving Network Partitions", DAC 1982
///  
/// ===================================================================

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <limbo/math/Math.h>
#include <boost/array.hpp>
#include <boost/unordered_map.hpp>

using std::cout;
using std::endl;
using std::vector;
using std::map;
using std::set;
using std::pair;
using std::make_pair;
using boost::array;
using boost::unordered_map;

namespace limbo { namespace algorithms { namespace partition {

/// \param NodeType, type of nodes in the graph
template <typename NodeType>
struct FM_node_traits
{
	typedef NodeType node_type;
	typedef typename node_type::tie_id_type tie_id_type;
	typedef typename node_type::weight_type node_weight_type;
	
	/// \return id of a node to sort nodes in each bucket
	static tie_id_type tie_id(node_type const& n)
	{return n.tie_id();}
	/// \return weight of a node for balancing constraint, e.g. area 
	static node_weight_type weight(node_type const& n)
	{return n.weight();}
};

/// \param NodeType indicates type of nodes in the graph 
/// Only support two partitions 
template <typename NodeType, typename NetWeightType = double>
class FM
{
	public:
		typedef NodeType node_type;
		typedef NetWeightType net_weight_type;
		typedef typename FM_node_traits<node_type>::node_weight_type node_weight_type;

		struct FM_node_type;
		struct FM_net_type;

		struct FM_node_type
		{
			node_type* pNode;
			vector<FM_net_type*> vNet;
			bool locked; ///< locked or not
			int partition; ///< partition id: 0 or 1, -1 for uninitialized
			int partition_bak; ///< back up partition for trial pass
			node_weight_type weight; ///< node weight for balancing constraint, like area

			FM_node_type()
			{
				pNode = NULL;
				locked = false;
				partition = -1;
				partition_bak = -1;
				weight = 0;
			}
			net_weight_type gain() const
			{
				net_weight_type g = 0;
				for (typename vector<FM_net_type*>::const_iterator itNet = vNet.begin(); 
						itNet != vNet.end(); ++itNet)
				{
					for (typename vector<FM_node_type*>::const_iterator itNode = (*itNet)->vNode.begin();
							itNode != (*itNet)->vNode.end(); ++itNode)
					{
						assert((*itNode)->partition == 0 || (*itNode)->partition == 1);

						if (this == *itNode) continue;
						else if (this->partition == (*itNode)->partition) // internal node 
							g -= (*itNet)->weight;
						else // external node
							g += (*itNet)->weight;
					}
				}
				return g;
			}
		};
		struct FM_net_type
		{
			vector<FM_node_type*> vNode;
			net_weight_type weight; ///< net weight
		};

		/// forward declaration
		struct compare_type1;
		struct compare_type2;
		/// largest gain comes first
		struct compare_type1
		{
			bool operator()(FM_node_type* pFMNode1, FM_node_type* pFMNode2) const 
			{
				return pFMNode1->gain() > pFMNode2->gain();
			}
			bool operator()(net_weight_type const& g1, net_weight_type const& g2) const
			{
				return g1 > g2;
			}
#if 0
			bool operator()(set<FM_node_type*, compare_type2> const& s1, set<FM_node_type*, compare_type2> const& s2) const
			{
				assert(!s1.empty() && !s2.empty());
				return (*this)(*s1.begin(), *s2.begin());
			}
#endif
		};
		/// smallest tie_id comes first
		struct compare_type2
		{
			bool operator()(FM_node_type* pFMNode1, FM_node_type* pFMNode2) const 
			{
				return FM_node_traits<node_type>::tie_id(*(pFMNode1->pNode)) < FM_node_traits<node_type>::tie_id(*(pFMNode2->pNode));
			}
		};

		/// gain bucket type
		class gain_bucket_type : public map<net_weight_type, set<FM_node_type*, compare_type2>, compare_type1>
		{
			public:
				typedef set<FM_node_type*, compare_type2> nested_type;
				typedef map<net_weight_type, nested_type, compare_type1> base_type;

				gain_bucket_type() : base_type() {}
				gain_bucket_type(gain_bucket_type const& rhs) : base_type(rhs) {}

				virtual void insert(FM_node_type* const& pFMNode)
				{
					net_weight_type gain = pFMNode->gain();
					typename base_type::iterator found = this->find(gain);
					if (found == this->end())
					{
						set<FM_node_type*, compare_type2> sTmp;
						sTmp.insert(pFMNode);
						this->base_type::insert(make_pair(gain, sTmp));
					}
					else
					{
						found->second.insert(pFMNode);
					}
				}
				virtual void erase(FM_node_type* const& pFMNode)
				{
					net_weight_type gain = pFMNode->gain();
					typename base_type::iterator found = this->find(gain);
					if (found != this->end())
					{
						if (found->second.empty()) return;
						else if (found->second.size() == 1 && found->second.count(pFMNode))
							this->base_type::erase(found);
						else 
							found->second.erase(pFMNode);
					}
				}
				size_t element_size() const 
				{
					size_t cnt = 0;
					for (typename base_type::const_iterator it1 = this->begin();
							it1 != this->end(); ++it1)
						for (typename nested_type::const_iterator it2 = it1->second.begin();
								it2 != it1->second.end(); ++it2)
							cnt += 1;
					return cnt;
				}
				void print() const 
				{
					cout << "------- gain_bucket -------" << endl;
					cout << "total # element = " << this->element_size() << endl;
					for (typename base_type::const_iterator it1 = this->begin();
							it1 != this->end(); ++it1)
					{
						cout << "gain = " << it1->first << ": ";
						for (typename nested_type::const_iterator it2 = it1->second.begin();
								it2 != it1->second.end(); ++it2)
							cout << FM_node_traits<node_type>::tie_id(*((*it2)->pNode)) << " ";
						cout << endl;
					}
				}
		};

		FM() {}
		~FM()
		{
			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
				delete it->second;
			for (typename vector<FM_net_type*>::const_iterator it = m_vNet.begin(); 
					it != m_vNet.end(); ++it)
				delete *it;
		}
		/// \param initialPartition, 0 or 1
		/// \return whehter insertion is successful
		bool add_node(node_type* pNode, int initialPartition)
		{
			assert(initialPartition == 0 || initialPartition == 1);

			FM_node_type* pFMNode = new FM_node_type;
			pFMNode->pNode = pNode;
			pFMNode->partition = initialPartition;
			pFMNode->weight = FM_node_traits<node_type>::weight(*pNode);
			return m_hNode.insert(make_pair(pNode, pFMNode)).second;
		}
		/// \param Iterator, dereference of which must be type of node
		/// \return whehter a net is successfully added
		/// this function must be called after all nodes are inserted
		/// if C++11 is allowed, we can extend it to variant length arguments
		template <typename Iterator>
		bool add_net(net_weight_type const& weight, Iterator first, Iterator last)
		{
			FM_net_type* pFMNet = new FM_net_type;
			pFMNet->weight = weight;

			for (Iterator it = first; it != last; ++it)
			{
				typename unordered_map<node_type*, FM_node_type*>::const_iterator found = m_hNode.find(*it);
				// return false if failed
				if (found == m_hNode.end())
				{
					delete pFMNet;
					return false;
				}
				// add FM_node_type to FM_net_type
				pFMNet->vNode.push_back(found->second);
			}
			// add FM_net_type to FM_node_type
			for (typename vector<FM_node_type*>::const_iterator itNode = pFMNet->vNode.begin(); 
					itNode != pFMNet->vNode.end(); ++itNode)
				(*itNode)->vNet.push_back(pFMNet);
			m_vNet.push_back(pFMNet);

			return true;
		}
		/// \return cutsize of current partition
		net_weight_type cutsize() const 
		{
			net_weight_type cs = 0;
			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
			{
				FM_node_type* const& pFMNode = it->second;
				assert(pFMNode->partition == 0 || pFMNode->partition == 1);

				for (typename vector<FM_net_type*>::const_iterator itNet = pFMNode->vNet.begin(); 
						itNet != pFMNode->vNet.end(); ++itNet)
				{
					for (typename vector<FM_node_type*>::const_iterator itNode = (*itNet)->vNode.begin(); 
							itNode != (*itNet)->vNode.end(); ++itNode)
					{
						if (pFMNode == *itNode) continue;
						cs += (pFMNode->partition^(*itNode)->partition)*(*itNet)->weight;
					}
				}
			}
			return cs/2;
		}
		/// top api for FM 
		/// \return final cutsize after partition
		net_weight_type operator()(double ratio1, double ratio2)
		{
			return this->run(ratio1, ratio2);
		}
		/// print function 
		void print() const 
		{
			cout << "------- partitions -------" << endl;
			vector<typename FM_node_traits<node_type>::tie_id_type> vPartition[2];

			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
			{
				FM_node_type* const& pFMNode = it->second;
				assert(pFMNode->partition == 0 || pFMNode->partition == 1);
				vPartition[pFMNode->partition].push_back(FM_node_traits<node_type>::tie_id(*(pFMNode->pNode)));
			}
			cout << "{";
			for (typename vector<typename FM_node_traits<node_type>::tie_id_type>::const_iterator it = vPartition[0].begin();
					it != vPartition[0].end(); ++it)
				cout << (*it) << " ";
			cout << "| ";
			for (typename vector<typename FM_node_traits<node_type>::tie_id_type>::const_iterator it = vPartition[1].begin();
					it != vPartition[1].end(); ++it)
				cout << (*it) << " ";
			cout << "}" << endl;
		}
		/// print connection
		void print_connection() const
		{
			cout << "------- connections -------" << endl;
			for (typename vector<FM_net_type*>::const_iterator itNet = m_vNet.begin();
					itNet != m_vNet.end(); ++itNet)
			{
				FM_net_type* const& pFMNet = *itNet;
				for (typename vector<FM_node_type*>::const_iterator itNode = pFMNet->vNode.begin();
						itNode != pFMNet->vNode.end(); ++itNode)
				{
					// delimiter
					if (itNode != pFMNet->vNode.begin())
						cout << " - ";
					FM_node_type* const& pFMNode = *itNode;
					cout << FM_node_traits<node_type>::tie_id(*(pFMNode->pNode));
				}
				cout << endl;
			}
		}
		/// print node 
		void print_node() const 
		{
			cout << "------- nodes -------" << endl;
			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
			{
				FM_node_type* const& pFMNode = it->second;
				assert(pFMNode->partition == 0 || pFMNode->partition == 1);

				cout << FM_node_traits<node_type>::tie_id(*(pFMNode->pNode)) << ": ";
				cout << "partition = " << pFMNode->partition << "; ";
				cout << "partition_bak = " << pFMNode->partition_bak << "; ";
				cout << "locked = " << pFMNode->locked << "; ";
				cout << "weight = " << pFMNode->weight << "; ";
				cout << "gain = " << pFMNode->gain() << "; ";
				cout << endl;
			}
		}
	protected:
		/// \param ratio1, minimum target ratio for partition 0 over partition 1
		/// \param ratio2, maximum target ratio for partition 0 over partition 1
		/// \return, final cut size
		net_weight_type run(double ratio1, double ratio2)
		{
			net_weight_type prev_cutsize;
			net_weight_type cur_cutsize = this->cutsize();

			unsigned int iter_cnt = 0;
			do 
			{
				cout << "######### iteration #" << iter_cnt++ << " #########" << endl;

				prev_cutsize = cur_cutsize;

				pair<net_weight_type, int> trial_pass = this->single_pass(ratio1, ratio2, -1);
#ifdef DEBUG_FM
				//this->print_node();
#endif
				pair<net_weight_type, int> actual_pass = this->single_pass(ratio1, ratio2, trial_pass.second);

#ifdef DEBUG_FM
				this->print_node();
				assert(trial_pass == actual_pass && actual_pass.first == this->cutsize());
#endif 

				cur_cutsize = actual_pass.first;
			} while (cur_cutsize < prev_cutsize);

			return cur_cutsize;
		}
		/// \param ratio1, minimum target ratio for partition 0 over partition 1
		/// \param ratio2, maximum target ratio for partition 0 over partition 1
		/// \param target_cnt, generalized iteration count, if it is negative, then run in trial mode 
		/// \return, pair of best cut size and iteration count
		pair<net_weight_type, int> single_pass(double ratio1, double ratio2, int target_cnt)
		{
			// trial mode, back up partition
			if (target_cnt < 0) 
			{
				cout << "======= trial phase =======" << endl;
				for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
						it != m_hNode.end(); ++it)
				{
					FM_node_type* const& pFMNode = it->second;
					pFMNode->partition_bak = pFMNode->partition;
				}
			}
			else cout << "======= actual phase w. target_cnt = " << target_cnt << " =======" << endl;
#ifdef DEBUG_FM
			//this->print_node();
#endif
			// initialize gain_bucket
			gain_bucket_type gain_bucket;
			node_weight_type total_weight[2] = {0, 0};
			net_weight_type cur_cutsize = this->cutsize();
			net_weight_type best_cutsize = cur_cutsize;
			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
			{
				FM_node_type* const& pFMNode = it->second;
				assert(pFMNode->partition == 0 || pFMNode->partition == 1);
				total_weight[pFMNode->partition] += pFMNode->weight;
				gain_bucket.insert(pFMNode);
			}

#ifdef DEBUG_FM
			// for Physical Design HW 2.b)
			gain_bucket.print();
#endif 

			for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
					it != m_hNode.end(); ++it)
			{
				FM_node_type* const& pFMNode = it->second;
				pFMNode->locked = false;
			}

#ifdef DEBUG_FM
			this->print();
			cout << "initial cutsize = " << this->cutsize() << endl;
#endif
			// 1 trial pass, 1 real pass
			int cur_cnt = 0;
			int best_cnt = 0;
			while (!gain_bucket.empty())
			{
				if (cur_cnt == target_cnt) break;
#ifdef DEBUG_FM
				//this->print_node();
#endif
				FM_node_type* pFMNodeBest = NULL; // candidnate node to move

				for (typename gain_bucket_type::const_iterator it1 = gain_bucket.begin(); 
						it1 != gain_bucket.end(); ++it1)
				{
					for (typename gain_bucket_type::nested_type::const_iterator it2 = it1->second.begin();
							it2 != it1->second.end(); ++it2)
					{
						FM_node_type* pFMNode = *it2;

						node_weight_type tmp_total_weight[2] = {
							total_weight[0],
							total_weight[1]
						};
						// move out from current partition
						tmp_total_weight[pFMNode->partition] -= pFMNode->weight;
						// move into the other partition
						tmp_total_weight[!pFMNode->partition] += pFMNode->weight;

						double cur_ratio = (double)total_weight[0]/total_weight[1];
						double tmp_ratio = (double)tmp_total_weight[0]/tmp_total_weight[1];

						// the ratio stays in the target range or get closer to the target range
						if (limbo::abs(tmp_ratio-ratio1)+limbo::abs(tmp_ratio-ratio2) 
								<= limbo::abs(cur_ratio-ratio1)+limbo::abs(cur_ratio-ratio2))
						{
							pFMNodeBest = pFMNode;
							break;
						}
					}
					if (pFMNodeBest) break;
				}
				// this is not exit condition yet
				if (!pFMNodeBest) break; 

#ifdef DEBUG_FM
				//gain_bucket.print();
#endif
				// move pFMNodeBest
				// move out from current partition
				total_weight[pFMNodeBest->partition] -= pFMNodeBest->weight;
				// move into the other partition
				total_weight[!pFMNodeBest->partition] += pFMNodeBest->weight;
				// remove pFMNodeBest and all its connected nodes from gain bucket 
				gain_bucket.erase(pFMNodeBest);
				for (typename vector<FM_net_type*>::const_iterator itNet = pFMNodeBest->vNet.begin(); 
						itNet != pFMNodeBest->vNet.end(); ++itNet)
				{
					for (typename vector<FM_node_type*>::const_iterator itNode = (*itNet)->vNode.begin(); 
							itNode != (*itNet)->vNode.end(); ++itNode)
					{
						// skip itself and locked nodes 
						if (*itNode == pFMNodeBest || (*itNode)->locked) continue;
#ifdef DEBUG_FM
						//cout << "erasing " << (*itNode)->pNode->tie_id() << endl;
#endif
						gain_bucket.erase(*itNode);
#ifdef DEBUG_FM
						//gain_bucket.print();
#endif
					}
				}
#ifdef DEBUG_FM
				//gain_bucket.print();
#endif
				// update current cut size 
				for (typename vector<FM_net_type*>::const_iterator itNet = pFMNodeBest->vNet.begin(); 
						itNet != pFMNodeBest->vNet.end(); ++itNet)
				{
					for (typename vector<FM_node_type*>::const_iterator itNode = (*itNet)->vNode.begin(); 
							itNode != (*itNet)->vNode.end(); ++itNode)
					{
						// skip itself
						if (*itNode == pFMNodeBest) continue;
						// directly update cut size 
						if (pFMNodeBest->partition == (*itNode)->partition)
							cur_cutsize += (*itNet)->weight;
						else 
							cur_cutsize -= (*itNet)->weight;
					}
				}
				// update partition of pFMNodeBest
				pFMNodeBest->partition = !pFMNodeBest->partition;
				pFMNodeBest->locked = true;
				// insert all its connected nodes 
				for (typename vector<FM_net_type*>::const_iterator itNet = pFMNodeBest->vNet.begin(); 
						itNet != pFMNodeBest->vNet.end(); ++itNet)
				{
					for (typename vector<FM_node_type*>::const_iterator itNode = (*itNet)->vNode.begin(); 
							itNode != (*itNet)->vNode.end(); ++itNode)
					{
						if (*itNode == pFMNodeBest || (*itNode)->locked) continue;
						gain_bucket.insert(*itNode);
					}
				}

				// record current cnt
				++cur_cnt;

#ifdef DEBUG_FM
				//gain_bucket.print();
				assert(cur_cutsize == this->cutsize());
				this->print();
				cout << "move cnt = " << cur_cnt << ", current cutsize = " << this->cutsize() << endl;
#endif

				if (cur_cutsize < best_cutsize)
				{
					best_cutsize = cur_cutsize;
					best_cnt = cur_cnt;
				}
			}
			// trial mode, recover partition
			if (target_cnt < 0)
			{
				for (typename unordered_map<node_type*, FM_node_type*>::const_iterator it = m_hNode.begin();
						it != m_hNode.end(); ++it)
				{
					FM_node_type* const& pFMNode = it->second;
					pFMNode->partition = pFMNode->partition_bak;
				}
			}

			return make_pair(best_cutsize, best_cnt);
		}

		unordered_map<node_type*, FM_node_type*> m_hNode; ///< FM nodes
		vector<FM_net_type*> m_vNet; ///< FM nets 
		gain_bucket_type m_gain_bucket; ///< gain buckets
};

}}} // namespace limbo  // namespace algorithms // namespace partition

#endif