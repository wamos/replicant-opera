#ifndef FLOW_SIM_SIMPLE_TOR_H
#define FLOW_SIM_SIMPLE_TOR_H
#include <cinttypes>
#include <vector>
#include <map>
#include <queue>
#include <iostream>
#include <numeric>
#include "flow.h"
#include "flowsim_config_macro.h"

struct TORSwitch {
    uint64_t tor_id;
    uint64_t nodes_per_rack;
    // If we're dealing with spare matrices, then we need to consider a different representation 
    std::vector<double> local_vector; // local traffic
    std::vector<std::vector<double>> buffer_matrix; // non-local traffic: src_racks * dst_racks
    std::vector<std::vector<double>> recv_offers;   // offers: src_racks * dst_racks
    std::vector<double> send_capacity; // "capacity" in RotorNet paper
    // internally used for RLB phase 2
    std::vector<double> recv_avail; // "avail" in RotorNet paper
    // internally used for RLB phase 2
    std::vector<std::vector<double>> offerscl; // offerscl: src_racks * dst_racks
    // the output of RLB phase 2
    std::vector<std::vector<double>> indir; // indir: src_racks * dst_racks
    // the start of RLB phase 3, src - >dst
    std::vector<double> accept; // accept per destination: local vector = local vector - accept
    // forward_amount of src -> mid
    std::vector<double> forward;

    // flow id -> flow size, local flows are flows having src hosts within the rack
    std::map<uint64_t, double> local_flows;
    // flow id -> flow size, buffer flows will be updated later
    std::map<uint64_t, double> buffer_flows;
    
    TORSwitch(){        
    }
    
    TORSwitch(uint64_t id)
    :tor_id(id)
    ,nodes_per_rack(NODES_PER_RACK){
    }

    void InitFlowMatrices(int num_rack){
        for(int host = 0; host < num_rack ; host++){
            buffer_matrix.push_back(std::vector<double>(num_rack));
            std::fill(buffer_matrix[host].begin(), buffer_matrix[host].end(), 0.0);

            recv_offers.push_back(std::vector<double>(num_rack));
            std::fill(recv_offers[host].begin(), recv_offers[host].end(), 0.0);

            indir.push_back(std::vector<double>(num_rack));
            std::fill(indir[host].begin(), indir[host].end(), 0.0);

            offerscl.push_back(std::vector<double>(num_rack));
            std::fill(offerscl[host].begin(), offerscl[host].end(), 0.0);

            local_vector.push_back(0.0);
            recv_avail.push_back(0.0);
            send_capacity.push_back(0.0);
            accept.push_back(0.0);
            forward.push_back(0.0);
        }
    }

    void clearFlowMatrices(int num_rack){
        for(int host = 0; host < num_rack ; host++){
            std::fill(buffer_matrix[host].begin(), buffer_matrix[host].end(), 0.0);
            std::fill(recv_offers[host].begin(), recv_offers[host].end(), 0.0);
            std::fill(indir[host].begin(), indir[host].end(), 0.0);
            std::fill(offerscl[host].begin(), offerscl[host].end(), 0.0);
        }
        std::fill(local_vector.begin(), local_vector.end(), 0.0);
        std::fill(recv_avail.begin(), recv_avail.end(), 0.0);
        std::fill(send_capacity.begin(), send_capacity.end(), 0.0);
        std::fill(accept.begin(), accept.end(), 0.0);
        std::fill(forward.begin(), forward.end(), 0.0);
    }

    void clearFlowStats(){
        local_flows.clear();
        buffer_flows.clear();
    }

    void initLocalFlow(Flow& flow, bool stopped_flag){
        uint64_t src_rack_id = flow.src_host / nodes_per_rack;
        uint64_t dst_rack_id = flow.dst_host / nodes_per_rack;
        if(src_rack_id == tor_id){
            std::cout <<"src:"<< flow.src_host <<", dst:" << flow.dst_host << "\n";
            std::cout << "total_bytes:" << flow.total_bytes << ", completed_bytes" << flow.completed_bytes << "\n";
            uint64_t remain_bytes;
            if(stopped_flag){
                 remain_bytes = 0;
            }
            else{
                remain_bytes = flow.total_bytes;
            }
            local_flows[flow.flow_id] = (double) remain_bytes;
            local_vector[dst_rack_id] = (double) remain_bytes;
        }
    }

    void addBufferFlow(Flow& flow, double val = 0.0){
        uint64_t rack_id = flow.src_host / nodes_per_rack;
        if(rack_id != tor_id){
            buffer_flows[flow.flow_id] = val;
        }
    }

    void incrementLocalFlow(uint64_t flow_id, double bytes){
        local_flows[flow_id] += bytes;
    }

    void incrementBufferFlow(uint64_t flow_id, double bytes){
        buffer_flows[flow_id] += bytes;
    }

    double getBufferFlowByID(uint64_t flow_id){
        return buffer_flows[flow_id];
    }

    double getLocalFlowByID(uint64_t flow_id){
        return local_flows[flow_id];
    }

    void decrementBufferFlow(uint64_t flow_id, double bytes){
        buffer_flows[flow_id] -= bytes;
    }

    void decrementLocalFlow(uint64_t flow_id, double bytes){
        local_flows[flow_id] -= bytes;
    }

    bool nonzeroOffersclColumns(std::vector<int>& nonzero_columns){
        bool nonzero = false;
        std::vector<double> column(offerscl[0].size());
        for(int col = 0; col< offerscl[0].size(); col ++){
            getOffersclColumn(col, column);
            double val = std::accumulate(column.begin(), column.end(), 0.0); 
            if(val > 0.125){
                nonzero = true;
                std::cout <<"hasNonzeroColumns\n";
                std::cout << std::fixed << "col:" << col << ", val:" << val << "\n"; 
                nonzero_columns.push_back(col);
            }   
        }
        return nonzero;
    }

    void getOffersclColumn(int col, std::vector<double>& vec){
        for(int row=0; row < offerscl.size(); row++){
            vec[row] = offerscl[row][col];
        }
    }

    double getBufferValue(int row, int col){
        return buffer_matrix[row][col];
    }

    double getColSumBuffer(int col){
        double sum = 0;  
        for(int row=0; row < buffer_matrix[0].size(); row++){
           sum += buffer_matrix[row][col];
        }
    }

    void getBufferRow(int row, std::vector<double>& vec){  
        for(int col=0; col < buffer_matrix[0].size(); col++){
           vec[col] = buffer_matrix[row][col];
        }
    }

    void getBufferColumn(int col, std::vector<double>& vec){    
        for(int row=0; row < buffer_matrix.size(); row++){
            vec[row] = buffer_matrix[row][col];
        }
    }

    // = -> +=
    void updateIndirColumn(int col, std::vector<double>& vec){    
        for(int row=0; row < indir.size(); row++){
            indir[row][col] += vec[row];
        }
    }

    void perRowSumIndir(std::vector<double>& vec){
        for(int row=0; row < indir.size(); row++){
            double row_sum = std::accumulate(indir[row].begin(),indir[row].end(), 0.0);
            vec[row] = row_sum;
        }
    }

    std::vector<double> getLocalVector(){
        return local_vector;
    }
    
    double getLocalValue(int dst){
        return local_vector[dst];
    }

    bool updateBufferValue(int row, int col, double val){
        buffer_matrix[row][col] = val;
        return true;
    }

    void updateLocalValue(int dst, double val){
        local_vector[dst] = val;
    }

    /*void printLocalVector(){
        for(auto e: local_vector){
            std::cout << e << ",";
        }
        std::cout << "\n";
    }

    void printSendCapacity(){
        for(auto e: send_capacity){
            std::cout << e << ",";
        }
        std::cout << "\n";
    }

    void printRecvAvail(){
        for(auto e: recv_avail){
            std::cout << e << ",";
        }
        std::cout << "\n";
    }

    void printOfferscl(){
        std::cout <<"Offerscl:\n";
        for(int i = 0 ; i < offerscl.size(); i++){
            for(int j = 0; j < offerscl[0].size();j ++){
                std::cout << offerscl[i][j] << ",";
            }
            std::cout << "\n";
        }
    }

    void printOffers(){
        std::cout <<"Offers:\n";
        for(int i = 0 ; i < recv_offers.size(); i++){
            for(int j = 0; j < recv_offers[0].size();j ++){
                std::cout << recv_offers[i][j] << ",";
            }
            std::cout << "\n";
        }
    }

    void printBufferMatrix(){
        for(int i = 0 ; i < buffer_matrix.size(); i++){
            for(int j = 0; j < buffer_matrix[0].size();j ++){
                std::cout << buffer_matrix[i][j] << ",";
            }
            std::cout << "\n";
        }
    }*/

};

#endif