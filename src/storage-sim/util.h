#ifndef FLOW_SIM_GENERAL_UTIL_H
#define FLOW_SIM_GENERAL_UTIL_H

#include <vector>
#include <limits>
#include <iostream>
//#include <algorithm>

//TODO: why we need inline here?
namespace util {
    /*template<class InputIt, class T>
    constexpr InputIt find(InputIt first, InputIt last, const T& value)
    {  
        for (; first != last; ++first) {
            if (*first == value) {
                return first;
            }
        }
        return last;
    }*/

    template<class T>
    inline void printMatrix(std::vector<std::vector<T>>& matrix, std::string mat_name){
        //bool all_zeros = true;
        std::cout << mat_name <<"\n";
        //assume matrix is n*n
        for(int i = 0 ; i < matrix.size(); i++){
            for(int j = 0; j < matrix[0].size();j ++){
                if(matrix[i][j] != 0){
                    //all_zeros = false;
                    std::cout << std::fixed << "[" << i << "," << j << "]:" << matrix[i][j] << "\n";
                }
            }
            //std::cout << "\n";
        }
        /*if(all_zeros){
            std::cout << "all zeros\n";
        }*/
    }


    template<class T>
    inline void printVector(std::vector<T>& vec, std::string vec_name){
        //bool all_zeros = true;
        std::cout << vec_name <<"\n";
        for(int i = 0 ; i < vec.size(); i++){
            if(vec[i] != 0){
                //all_zeros = false;
                std::cout << std::fixed << "[" << i << "]:" << vec[i] << "\n" ;
            }
        }
        /*if(all_zeros){
            std::cout << "all zeros\n";
        }*/
        //std::cout << "\n";
    }

    /***
    Max-min fair allocation
    1. maximizes the minimum rate
    2. can be viewed as giving the maximum protection to the
    minimum of the alloted rates (absolute property)
    3. All unsatisfied sources get the same rate which means that
    there is no incentive for a source to benefit from inflating its
    required rate
    ***/

    inline std::vector<double> fairshare1d(std::vector<double> input, double cap1, bool extra){
        std::vector<double> sent(input.size());
        int nelem = 0;
        if(cap1==0){
            std::fill(sent.begin(), sent.end(), 0);
            return sent;
        }

        for (int i = 0; i < input.size(); i++){
            if (input[i] > 0){
                nelem++;
            }
        }

        /*for (int i = 0; i < input.size(); i++) {
                std::cout << "input[i]:" << input[i] << "\n";
        }*/

        if (nelem != 0) {
            double pre_share = 0; //f_share;
            while(nelem > 0){
                double updated_share = cap1 / (double) nelem; // new share
                //std::cout << "updated_share:" << updated_share << "\n";
                for(int i = 0 ; i < input.size(); i++){
                    if (input[i] > 0 && input[i] <= updated_share) {
                        cap1 = cap1 - input[i];
                        sent[i] = input[i];
                        input[i] = 0;
                        nelem--;
                    }

                    if(updated_share == pre_share && input[i] > 0){
                        cap1 = cap1 - updated_share;
                        sent[i] = updated_share;
                        nelem--;
                    }
                }
                pre_share = updated_share;
            }
        }
        else{
            std::fill(sent.begin(), sent.end(), 0);
        }
        return sent;
    }

    inline std::vector<std::vector<double>> fairshare2d(std::vector<std::vector<double>> input, std::vector<double> cap0, std::vector<double> cap1){
        // if we take `input` as an N x M matrix (N rows, M columns)
        // then cap0[i] is the capacity of the sum of the i-th row
        // and cap1[i] is the capacity of the sum of the i-th column

        std::vector<std::vector<double>> sent(input.size());
        std::vector<std::vector<double>> sent_temp(input.size());
        for (int i = 0; i < input.size(); i++) {
            sent[i].resize(input[0].size());
            sent_temp[i].resize(input.size());
        }

        for (int i = 0; i < input.size(); i++) { // sweep rows (i): (cols j)
            sent_temp[i] = fairshare1d(input[i], cap0[i], true);
        }

        /*std::cout<< "sent_temp\n";
        for (int i = 0; i < input.size(); i++) {
            for (int j = 0; j < input[i].size(); j++) {
                std::cout << sent_temp[i][j] << ",";
            }
            std::cout<< "\n";
        }*/

        for (int i = 0; i < input[0].size(); i++) { // sweep columns
            std::vector<double> col_vector(input.size());
            for(int j = 0; j < input.size(); j++){
                col_vector[j] = sent_temp[j][i];
            }

            /*std::cout<< "col_vector\n";
            for(auto e: col_vector){
                std::cout << e << ",";
            }
            std::cout<< "\n";*/

            auto fair_col = fairshare1d(col_vector, cap1[i], true);

            /*std::cout<< "fair_col\n";
            for(auto e: fair_col){
                std::cout << e << ",";
            }
            std::cout<< "\n";*/

            for(int j = 0; j < input.size(); j++){
                sent[j][i] = fair_col[j];
            }
        }
        return sent;
    }
}

#endif