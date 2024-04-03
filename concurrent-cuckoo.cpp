#include <vector>
#include <list>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <algorithm>
#include <iostream>
#include <functional>
#include <future>
#include <cstdlib>
#include <type_traits>

template<typename T>
class PhasedCuckooHashSet {
private:
    std::vector<std::vector<std::list<T>>> table;
    std::vector<std::vector<std::recursive_mutex*>> locks;
    int capacity;
    const int PROBE_SIZE = 4;
    const int THRESHOLD = 2;
    const int LIMIT = 60;

    std::random_device rd;
    std::mt19937 gen;

 

   // std::mutex contains_lock;

    // Function to generate a random integer between min and max
    int rand_int(int min, int max) {
        std::uniform_int_distribution<> rand_dist(min, max);
        return rand_dist(gen);
    }

    // Function to generate a random float between min and max
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

    
    // Translate floats and other data types into a string of bytes in order to hash
    // Standard hash
    int hash0(const T& x) {
        std::uint64_t hashed_value = std::hash<T>{}(x);
        return hashed_value;
    }

    uint64_t hash1(const T& x) {
        std::uint64_t hashed_value = std::hash<T>{}(x);
        return hashed_value / 2;
    }


    // Populate with random integers
    void populate() {
        int half = capacity/2;
        if (std::is_same<T, int>::value) {
            // Populate with random integers
            for(int i = 0; i < half; ++i) {
                add(rand_int(0, 10000));
            }
        } else if (std::is_same<T, float>::value) {
            // Populate with random floats
            for(int i = 0; i < half; ++i) {
                add(rand_float(0.0f, 10000.0f));
            }
        } else if (std::is_same<T, double>::value) {
            // Populate with random doubles
            for(int i = 0; i < half; ++i) {
                add(rand_doub(0.0, 10000.0));
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
            return rand_int(0, 10000);
        } else if (std::is_same<T, char>::value) {
            return rand_char();
        } else if (std::is_same<T, float>::value) {
            return rand_float(0.0f, 1000000.0f);
        } else if (std::is_same<T, double>::value){
            return rand_doub(0.0, 1000000.0);
        }
    }


public:
    PhasedCuckooHashSet(int size) : capacity(size){

        locks.resize(2);
        table.resize(2);

        for (int i = 0; i < 2; ++i) {
            locks[i].resize(capacity);
            table[i].resize(capacity);
            for (int j = 0; j < capacity; ++j) {
                table[i][j] = std::list<T>();
                locks[i][j] = new std::recursive_mutex();
            }
        }
    }

    bool contains(const T& x) {
        acquire(x);
        //std::lock_guard<std::mutex> lock(contains_lock); // Lock the mutex
        // Check both tables for the element
        for (int i = 0; i < 2; ++i) {
            int h = (i == 0) ? hash0(x) : hash1(x);
            auto& set = table[i][h % capacity];
            for (const auto& item : set) {
                if (item == x) {
                    release(x);
                    return true;
                }
            }
        }
        release(x);
        return false;
    }

    bool remove(const T& x) {
        acquire(x);

        auto& set0 = table[0][hash0(x) % capacity];
        auto& set1 = table[1][hash1(x) % capacity];

        if (std::find(set0.begin(), set0.end(), x) != set0.end()) {
            set0.remove(x);
            release(x);
            return true;
        } else if (std::find(set1.begin(), set1.end(), x) != set1.end()) {
            set1.remove(x);
            release(x);
            return true;
        }

        release(x);
        return false;
    }

    bool relocate(int i, int hi) {
        int hj = 0;
        int j = 1 - i;
        for (int round = 0; round < LIMIT; round++) {
            auto& iSet = table[i][hi];
            T y = iSet.front();
            switch (i) {
                case 0: hj = hash1(y) % capacity; break;
                case 1: hj = hash0(y) % capacity; break;
            }
            acquire(y);
            auto& jSet = table[j][hj];
            
            if (!iSet.empty() && iSet.front() == y) {
                iSet.pop_front();
                if (jSet.size() < THRESHOLD) {
                    jSet.push_back(y);
                    return true;
                } else if (jSet.size() < PROBE_SIZE) {
                    jSet.push_back(y);
                    i = 1 - i;
                    hi = hj;
                    j = 1 - j;
                } else {
                    iSet.push_back(y);
                    return false;
                }
            } else if (iSet.size() >= THRESHOLD) {
                continue;
            } else {
                return true;
            }

            release(y);
        }
        return false;
    }

    bool add(const T& x) {
        
        acquire(x);

        if (contains(x)){
            release(x);
            return false;
        } 

        int h0 = hash0(x) % capacity;
        int h1 = hash1(x) % capacity;
        //std::cout << h0 << "," << h1 << "\n";

        int i = -1;
        int h = -1;
        bool mustResize = false;

       
        auto& set0 = table[0][h0];
        auto& set1 = table[1][h1];
        //std::cout << "SIZES: " << set0.size() << "," << set1.size() << "\n";

        if (set0.size() < THRESHOLD) {
            set0.push_back(x);
            //std::cout<<"1\n";
            release(x);
            return true;
        } else if (set1.size() < THRESHOLD) {
            set1.push_back(x);
            //std::cout<<"2\n";
            release(x);
            return true;
        } else if (set0.size() < PROBE_SIZE) {
            set0.push_back(x);
            i = 0;
            h = h0;
            //std::cout<<"3\n";
        } else if (set1.size() < PROBE_SIZE) {
            set1.push_back(x);
            i = 1;
            h = h1;
            //std::cout<<"4\n";
        } else {
            
            mustResize = true;
        }
       
        release(x);
        
        if (mustResize) {
            resize();
            add(x);
        } else if (!relocate(i, h)) {
            resize();
        }
        return true; 
    }

    void acquire(const T& x) {
        locks[0][hash0(x) % locks[0].size()]->lock();
        locks[1][hash1(x) % locks[1].size()]->lock();
    }

    void release(const T& x) {
        locks[0][hash0(x) % locks[0].size()]->unlock();
        locks[1][hash1(x) % locks[1].size()]->unlock();
    }

    void resize() {
        int oldCapacity = capacity;

        // Locks are acquired
        for (auto& aLock : locks[0]) {
            aLock->lock();
        }

        if (capacity != oldCapacity) {
            return;
        }

        auto oldTable = table;
        capacity = 2*capacity;
        std::vector<std::vector<std::list<T>>> newTable(2, std::vector<std::list<T>>(capacity));
        table = newTable;
          
        for (auto& row : oldTable) {
            for (auto& set : row) {
                for (auto& z : set) {
                    
                    add(z);
                }
            }
        }
    
        for (auto& aLock : locks[0]) {
            aLock->unlock();
        }
       
    }

    int size() {
       int totalSize = 0;
        for (const auto& row : table) {
            for (const auto& list : row) {
                totalSize += list.size();
            }
        }
        std::cout<<"\nActual size: "<<totalSize << "\n"<<std::endl;
        return totalSize;
    }

	std::pair<int,int> do_work(int iters){
		
        int local = 0;
        
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
                    local += 1;
                }
			} else {
                if(remove(random_value())){
                    local -= 1;
                }
            }
		}

		// Get end time
		auto end_time = std::chrono::high_resolution_clock::now();

		// Calculate total execution time
    	auto exec_time_i = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

		
		return {exec_time_i, local};
	}

    void run(int num_threads, int iters) {
		
		// Populate the entire map 
		populate();

        // Get the initial size of the table
        int exp_size = size();
        
        // Start executing each thread
		std::vector<std::future<std::pair<int, int>>> futures;
		for (int i = 0; i < num_threads; ++i) {
			futures.emplace_back(std::async(std::launch::async, [&]() { return this->do_work(iters); }));
		}
        
		// Wait for all threads to finish and collect exec_time_i
		std::vector<int> exec_times;
		for (auto& future : futures) {
            auto result = future.get();
			exec_times.push_back(result.first);
            exp_size += result.second;
		}


		// Print execution time for each thread
		for (int i = 0; i < exec_times.size(); ++i) {
			std::cout << "Thread "<<i<< " time: " << exec_times[i] << "\n" << std::endl;
    	}
        
        std::cout<<"Expected: " << exp_size << "\n" << std::endl;

        // Print the actual size of the table
        size();
	}
};



int main() {
    int threads = 6;
    int iters = 1666;
    int size = 2500;

    PhasedCuckooHashSet<int> cuckooHashSet(size); 
    cuckooHashSet.run(threads, iters);

    return 0;
}