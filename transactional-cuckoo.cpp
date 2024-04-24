#include <iostream>
#include <vector>
#include <functional>
#include <random>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <future>
using namespace std;

template<typename T>
class CuckooHashSet {
private:
    static const int LIMIT = 30; 
    int capacity;
    std::vector<T> table0;
    std::vector<T> table1;
    std::random_device rd;
    std::mt19937 gen;

    int hash0(const T& x) {
        std::uint64_t hashed_value = std::hash<T>{}(x);
        return hashed_value;
    }

    uint64_t hash1(const T& x) {
        std::uint64_t hashed_value = std::hash<T>{}(x);
        return hashed_value / 2;
    }

    int rand_int(int min, int max) {
        std::uniform_int_distribution<> rand_dist(min, max);
        return rand_dist(gen);
    }

    float rand_float(float min, float max) {
        std::uniform_real_distribution<float> rand_dist(min, max);
        return rand_dist(gen);
    }

    // Function to generate a random double between min and max
    double rand_doub(double min, double max) {
        std::uniform_real_distribution<double> rand_dist(min, max);
        return rand_dist(gen);
    }

    // Function to generate a random char
    char rand_char() {
        std::uniform_int_distribution<int> rand_dist(0, 127);
        return static_cast<char>(rand_dist(gen));
    }

    void populate() {
        int half = capacity/2;
        if (std::is_same<T, int>::value) {
            // Populate with random integers
            for(int i = 0; i < half; ++i) {
                add(rand_int(0, 1000000));
            }
        } else if (std::is_same<T, float>::value) {
            // Populate with random floats
            for(int i = 0; i < half; ++i) {
                add(rand_float(0.0f, 1000000.0f));
            }
        } else if (std::is_same<T, double>::value) {
            // Populate with random doubles
            for(int i = 0; i < half; ++i) {
                add(rand_doub(0.0, 1000000.0));
            }
        } else if (std::is_same<T, char>::value) {
            // Populate with random chars
            for(int i = 0; i < half; ++i) {
                add(rand_char());
            }
        }
    }

    T random_value() {
        if (std::is_same<T, int>::value) {
            return rand_int(0, 100000);
        } else if (std::is_same<T, char>::value) {
            return rand_char();
        } else if (std::is_same<T, float>::value) {
            return rand_float(0.0f, 100000.0f);
        } else if (std::is_same<T, double>::value){
            return rand_doub(0.0, 100000.0);
        }
    }


public:
    CuckooHashSet(int k) : capacity(k) {
        table0.resize(capacity);
        table1.resize(capacity);
    }

    void resize() {
        capacity *= 2;
        table0.resize(capacity);
        table1.resize(capacity);
    }

    bool contains(const T& x) {
        return (table0[hash0(x) % capacity] == x) || (table1[hash1(x) % capacity] == x);
    }

    T swap(std::vector<T>& table, size_t hash_value, const T& x) {
        size_t capacity = table.size();
        size_t index = hash_value % capacity;

        T temp = std::move(table[index]); // Move the element at index to temp
        table[index] = x;                 // Place x at index

        return temp;  // Return the moved element
    }


    bool add(const T& x) {
        if (contains(x)) {
            return false;
        }

        for (int i = 0; i < LIMIT; ++i) {
            __transaction_atomic {
                if (swap(table0, hash0(x), x) == T()) {
                    return true;
                } else if (swap(table1, hash1(x), x) == T()) {
                    return true;
                }
            }
        }

        
        resize();
        add(x);
        

        return true;
    }

    bool remove(const T& x) {
        __transaction_atomic {
            if (table0[hash0(x) % capacity] == x) {
                //table0[hash0(x) % capacity] = T();
                table0.erase(table0.begin()+(hash0(x) % capacity));
                return true;
            } else if (table1[hash1(x) % capacity] == x) {
                //table1[hash1(x) % capacity] = T();
                table1.erase(table1.begin()+(hash1(x) % capacity));
                return true;
            }
        }
        return false;
    }

    int size() {
        int total = 0;
        __transaction_atomic {
            for (const auto& element : table0) {
                if(element != 0){
                    total+=1;
                }
                
            }
            for (const auto& element : table1) {
                if(element != 0){
                    total+=1;
                }
            }
        }
        std::cout<<total << "\n"<<std::endl;
        return total;
    }


    void do_work(){
        std::cout<<"Initial size: ";
        int exp = size();
        int iters = 75000;
        
		auto start_time = std::chrono::high_resolution_clock::now();

		// Loop iters number of times
		for (int i = 0; i < iters; ++i) {
			// Move random number generation inside the loop
			float prob = rand_float(0.0, 1.0);
			
			// Call contains with 80% probability
			if (prob < 0.8) {
				contains(random_value());
			} else if (prob >= 0.8 && prob < 0.9) {
				if(add(random_value())){
                    exp += 1;
                }
			} else {
                if(remove(random_value())){
                    exp -= 1;
                }
            }
		}

		// Get end time
		auto end_time = std::chrono::high_resolution_clock::now();

		// Calculate total execution time
    	auto exec_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

		std::cout<<"Execution time: "<<exec_time << "\n" << std::endl;

        std::cout<<"Actual Size: " << exp << "\n" << std::endl;
	}

    void run() {
		
		// Populate the entire map 
		populate();
        
        // Start executing each thread
		do_work();
	
        // Print the actual size of the table
        std::cout<<"Final size: ";
        size();
	}
        
};

int main() {
    CuckooHashSet<int> cuckooSet(50000);
    cuckooSet.run();
    

    return 0;
}