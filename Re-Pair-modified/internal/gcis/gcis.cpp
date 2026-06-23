#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>
#include "gcis.h"
#include "../bitvector.h"
using namespace std;

template <typename T>
bitVector GCIS<T>::setTypes()
{     
    size_t n = input.size();
    bitVector types(n, 0); 
    
    // 0 = S, 1 = L
    types.set0(n - 1); 

    for (long long i = n - 2; i >= 0; i--)
    {
        if (input[i] < input[i + 1])
        {
            types.set0(i);
        }
        else if (input[i] > input[i + 1])
        {
            types.set1(i);
        }
        else
        {
            (types[i + 1] == 1) ? types.set1(i) : types.set0(i);
        }
    }
    return types;
}

template <typename T>
vector<T> GCIS<T>::setPositions()
{
    vector<T> positions;
    if (types.size() < 2) return positions;
    if (types[0] == 0)
    {
        positions.push_back(0);
    }
    for (size_t i = 1; i < types.size(); i++)
    {
        if (types[i] == 0 && types[i - 1] == 1)
        {
            positions.push_back(static_cast<T>(i));
        }
    }
    return positions;
}

template <typename T>
void GCIS<T>::setLMS_distance()
{
    if (this->positions.empty()) return;

    size_t input_size = types.size();
    this->lms_distance.resize(input_size, 0);

    size_t len = 0;
    long long j = this->positions.size() - 1;
    for (long long i = input_size; i > 0; i--)
    {
        len++;
        if (this->positions[j] == static_cast<T>(i - 1))
        {
            this->lms_distance[i - 1] = static_cast<T>(len);
            len = 1;
            if (j > 0) j--;
            continue;
        } 
        this->lms_distance[i - 1] = static_cast<T>(len);
    }
}

template <typename T>
bool GCIS<T>::LMSequal(size_t pos1, size_t pos2)
{
    if (pos1 == pos2) return true;
    
    size_t len1 = this->lms_distance[pos1];
    size_t len2 = this->lms_distance[pos2];

    if (len1 != len2) return false;

    for (size_t i = 0; i < len1; i++)
    {
        if (input[pos1 + i] != input[pos2 + i] || types[pos1 + i] != types[pos2 + i])
        {
            return false;
        }
    }
    return true;
}

template <typename T>
bool GCIS<T>::isLMS(size_t pos)
{
    if (pos == 0 && types[pos] == 0)
    {
        return true;
    }
    if (pos == 0)
    {
        return false;
    }
    
    return types[pos] == 0 && types[pos - 1] == 1;
}

template <typename T>
void GCIS<T>::naming()
{
    vector<T> tails = bucket_tails; 
    for (long long i = positions.size() - 1; i >= 0; i--)
    {
        size_t pos = positions[i];
        SA[--tails[input[pos]]] = static_cast<signed_T>(pos);
    }
}

template <typename T>
void GCIS<T>::induce_l()
{
    vector<T> heads = bucket_heads;    
    for (size_t i = 0; i < SA.size(); i++)
    {
       if (SA[i] != -1 && SA[i] > 0)
        { 
            signed_T prev_pos = SA[i] - 1;
            if (types[prev_pos])
            {
                SA[heads[input[prev_pos]]++] = prev_pos;
            }
        }
    }
}

template <typename T>
void GCIS<T>::induce_s()
{
    vector<T> tails = bucket_tails;
    for (long long i = SA.size() - 1; i >= 0; i--)
    {
        if (SA[i] > 0)
        {
            signed_T prev_pos = SA[i] - 1;
            if (!types[prev_pos])
            {
                SA[--tails[input[prev_pos]]] = prev_pos;
            }
        }
    }
}

template <typename T>
void GCIS<T>::setBuckets()
{
    vector<size_t> count(alphabet_size, 0);
    
    for (auto c : input)
    {
        count[c]++;
    }
    
    this->bucket_heads.assign(alphabet_size, 0);
    this->bucket_tails.assign(alphabet_size, 0);

    size_t sum = 0;
    
    for (size_t i = 0; i < alphabet_size; i++)
    {
        bucket_heads[i] = static_cast<T>(sum);
        sum += count[i];        
        bucket_tails[i] = static_cast<T>(sum);
    }
}

template <typename T>
void GCIS<T>::findCFGandRules()
{
    long long prev_lms = -1;
    size_t first_lms_pos = positions.empty() ? input.size() : positions[0];
    long long current_name = 1;
    size_t cfg_size = positions.size() + (first_lms_pos > 0 ? 1 : 0);
    this->CFG.assign(cfg_size, -1);
    
    size_t rules_before_level = this->global_lcps.size();

    if (first_lms_pos > 0)
    {
        this->global_lcps.push_back(0);
        this->global_suffix_sizes.push_back(first_lms_pos);
        for (size_t k = 0; k < first_lms_pos; ++k)
        {
            this->global_suffixes.push_back(input[k]);
        }
        
        CFG[0] = 0; 
        current_name = 2;
    }

    long long last_unique_lms = -1;

    for (size_t i = 0; i < SA.size(); i++)
    {
        signed_T pos = SA[i]; 
        if (pos < 0 || !isLMS(pos)) continue;        
        auto it = lower_bound(positions.begin(), positions.end(), static_cast<T>(pos));
        size_t positions_idx = distance(positions.begin(), it);
        size_t cfg_index = positions_idx + (first_lms_pos > 0 ? 1 : 0);

        if (prev_lms == -1)
        {
            CFG[cfg_index] = static_cast<T>(current_name - 1); 
            
            this->global_lcps.push_back(0);
            this->global_suffix_sizes.push_back(lms_distance[pos] - 1);
            for (size_t k = 0; k < lms_distance[pos] - 1; ++k)
            {
                this->global_suffixes.push_back(input[pos + k]);
            }
            last_unique_lms = pos;
        }
        else if (LMSequal(prev_lms, pos)) 
        {
            CFG[cfg_index] = static_cast<T>(current_name - 1);
        }
        else
        {
            CFG[cfg_index] = static_cast<T>(current_name);
            
            size_t lcp_val = 0;
            size_t len_current = lms_distance[pos] - 1;
            
            if (prev_lms != -1)
            {
                size_t len_prev = lms_distance[last_unique_lms] - 1;
                size_t max_match = min(len_prev, len_current);
                while (lcp_val < max_match && input[last_unique_lms + lcp_val] == input[pos + lcp_val])
                {
                    lcp_val++;
                }
            }

            this->global_lcps.push_back(lcp_val);
            size_t suffix_sz = len_current - lcp_val;
            this->global_suffix_sizes.push_back(suffix_sz);

            for (size_t k = lcp_val; k < len_current; ++k)
            {
                this->global_suffixes.push_back(input[pos + k]);
            }
            
            current_name++;
            last_unique_lms = pos;
        }

        prev_lms = pos;
    }
    
    size_t rules_added_in_level = this->global_lcps.size() - rules_before_level;
    this->global_level_rule_counts.push_back(static_cast<uint64_t>(rules_added_in_level));
    
    this->level++;
    this->alphabet_size = current_name + 1; 
}

template <typename T>
void GCIS<T>::gen()
{
    this->types = setTypes();
    this->positions = setPositions();
    this->setLMS_distance();   
    this->setBuckets();

    this->SA.assign(input.size(), -1);
    
    this->naming();
    this->induce_l();
    this->induce_s();
    
    bucket_heads.clear();
    bucket_heads.shrink_to_fit();
    bucket_tails.clear();
    bucket_tails.shrink_to_fit();
    this->findCFGandRules();
    SA.clear();
    SA.shrink_to_fit();
    lms_distance.clear();
    lms_distance.shrink_to_fit();
    positions.clear();
    positions.shrink_to_fit();
}


template class GCIS<uint64_t>;
template class GCIS<uint32_t>;
template class GCIS<uint16_t>;