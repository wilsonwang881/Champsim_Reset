// WL 
#include "cache.h"
#include <unordered_set>
#include <map>
#include <cassert>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <vector>
#include <deque>
//#include <operable.h>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 256 
#define NUMBER_OF_PREFETCH_UNIT 400
#define HISTORY_SIZE 2000
#define CUTOFF 1
#define trainDataSize 25
#define testDataSize 975
#define totalDataSize 1000
#define K 3

namespace {
    //uint64_t champsim::operable::knn_accuracy=0;
    float final_accuracy=0;
    int number_of_prefetch=0;

    long saved=0;
    int special_page=0;
    int no_replace=0;
    int *store_page[20]={0};

    typedef struct{
      uint64_t point_1;
      uint64_t point_2;
      uint64_t classification;
    } ClassifiedPoint;

    typedef struct{
      uint64_t classification;
      uint64_t distance;
    } ClassifiedDistance;

  struct tracker {

    std::unordered_set<uint64_t> uniq_page_address;
    std::unordered_set<uint64_t> uniq_prefetched_page_address;
    std::deque<std::pair<uint64_t, uint64_t>> past_accesses;
    std::deque<ClassifiedPoint> previous_train;
    std::deque<ClassifiedPoint> trainData;
    std::deque<ClassifiedPoint> testData;
    std::deque<ClassifiedPoint> finalData;
    std::deque<ClassifiedPoint> current_train; 
    bool context_switch_toggled = false;
    public:


    bool context_switch_prefetch_gathered;

    std::deque<std::pair<uint64_t, bool>> context_switch_issue_queue;
    std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> context_switch_prefetching_timing; 

    //newly added

    bool check_within_block(uint64_t ref, uint64_t addr)
    {

      if (addr >= ref &&
          addr < (ref + PREFETCH_UNIT_SIZE)) {

          return true;
      }

      return false;
    }

    void gather_context_switch_prefetches(std::vector<uint64_t> & real_prefetch)
    {
      //uniq_page_address.clear();
      context_switch_issue_queue.clear();

      for(int i=0;i<real_prefetch.size();i++)
      {
        for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
        {
          //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
          uint64_t pf_addr = (real_prefetch[i]<<LOG2_PAGE_SIZE)+x*64;
          //prefetch_line(pf_addr, true, metadata_in);
          context_switch_issue_queue.push_back({pf_addr,true});
        }
      }
    }

    bool context_switch_queue_empty()
    {
      return context_switch_issue_queue.empty();
    }

    void context_switch_issue(CACHE* cache)
    {
      // Issue eligible outstanding prefetches
      if (!std::empty(context_switch_issue_queue)) {//&& champsim::operable::cache_clear_counter == 7
        auto [addr, priority] = context_switch_issue_queue.front();

        // If this fails, the queue was full.
        assert(priority);
        bool prefetched = cache->prefetch_line(addr, priority, 0);

        if (prefetched) {
          context_switch_issue_queue.pop_front();
          //context_switch_prefetching_timing.push_back({addr, cache->current_cycle, 0});

          //if (uniq_prefetched_page_address.find(addr >> 12) ==  uniq_prefetched_page_address.end()) {
            //std::cout << "First prefetch in page " << std::hex << addr << " prefetched at cycle " << std::dec << cache->current_cycle << std::endl;
            //uniq_prefetched_page_address.insert(addr >> 12); 
          //}
        }
      }
    }


  //newly added

    static int cmpfunc (const void*a,const void*b)
    {
      return(*(uint64_t*)a-*(uint64_t*)b);
    }

    std::vector<uint64_t> distinct_page ( uint64_t* arr ,int n) //finding distinct pages for every 1000 points
    {
      std::vector<uint64_t> special_page;
      special_page.clear(); 
      qsort(arr,n,sizeof(uint64_t),cmpfunc);
      for(int i=0;i<n;i++)
      {
        while (i<n-1 && arr[i]==arr[i+1])
        {
          i++;  
        }
        special_page.push_back(arr[i]);
       // std::cout<<"The special pages are"<<arr[i]<<std::endl;
      }
      return special_page;
      //printf("The number of distinct page is:%d\n",special_page);
    }

    uint64_t eukl_distance(int dimensions,uint64_t* point1, uint64_t* point2)
    {//calculate the distance between the points
    uint64_t temp = 0;
      for(int i = 0; i < dimensions; i++){
        temp += pow(point1[i] - point2[i], 2.0);
      }
      return sqrt(temp);
    }

    static int compareCdistances(const void* a, const void* b){
    ClassifiedDistance d1 = *((ClassifiedDistance*) a);
    ClassifiedDistance d2 = *((ClassifiedDistance*) b);

    if(d1.distance == d2.distance) return 0;
    else if(d1.distance < d2.distance) return -1;
    else return 1;
    }

    void sortCD(ClassifiedDistance* data, int size){
      qsort(data, size, sizeof(ClassifiedDistance), compareCdistances);
    }

    ClassifiedPoint convertToIrisPoint(int x){
      ClassifiedPoint point {};
      uint64_t pos[2];
      //before clk
      pos[0] = reset_misc::before_reset_on_demand_ins_access[x].cycle;
      //before adr
      pos[1] = reset_misc::before_reset_on_demand_ins_access[x].ip;
      point.point_1= pos[0];
      point.point_2 =pos[1];
      //Target
      uint64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
      //uint64_t page_num= (reset_misc::after_reset_on_demand_data_access[x].ip)>>12;
      point.classification = page_num;
      return point;
    }

    ClassifiedPoint convertToIrisPoint_aft_2 (int x)
    {
      ClassifiedPoint point {};
      //uint64_t * pos = (uint64_t *)malloc(2 * sizeof(uint64_t));
      uint64_t pos[2];
      //before clk
      pos[0] = reset_misc::before_reset_on_demand_ins_access[x].cycle;
      //pos[0] = reset_misc::before_reset_on_demand_data_access[x].cycle;
      //before adr
      pos[1] = reset_misc::before_reset_on_demand_ins_access[x].ip;
  
      point.point_1= pos[0];
      point.point_2 =pos[1];

      return point;
    }

    ClassifiedPoint convertToIrisPoint_only_page_num (int x)
    {
      ClassifiedPoint point {};
  
      //Target
      uint64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
      //uint64_t page_num= (reset_misc::after_reset_on_demand_data_access[x].ip)>>12;
      point.classification = page_num;

      return point;
    }

    void readTrainingData(int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size)
    {

      int i = 0;
      //Randomnize data
      int x =0;
      int record[train_size];
      uint64_t page_number[numOfLines];
 
      std::deque<ClassifiedPoint> train_save (train_size);
      //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
      std::deque<ClassifiedPoint> overall (total_size);
      train_save.clear();
      overall.clear();
 

      for (int i=0;i<total_size;i++)
      {
        ClassifiedPoint p = convertToIrisPoint(i);
        //overall[i] = *p;
        overall.push_back(p);
        page_number[i]=overall[i].classification;
      }

      i=0;
      int match=0;
      int no_match;
      for (int z=0;z<train_size;z++)
      {
        //x=rand()%999;
        x=(1000/train_size)*(z);
       // std::cout<<"train index"<<x<<std::endl;
        train[z]=overall[x];
        train_save.push_back(train[z]);
        record[z]=x;

      }
      while(i<total_size)
      {
        no_match=0;
        for(int j=0;j<train_size;j++)
        {
          if(i!=record[j])
          {
            no_match++;
          }
          if(i==record[j])
          {
            match++;
            printf("Number of match is:%d\n",match);
            i++;
            break;
          }
          if(no_match==train_size)
          {
            test[i-match]=overall[i-match];
            //test.at(i-match)=overall[i-match];
            int y=i-match;
            //printf("The test iteration is:%d\n",y);
            i++;
          }
        }
      }
  //return train_save;
    }

    void readTrainingData_aft_2 (int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size)
    {

      int i = 0;
      //Randomnize data
      int x =0;
      int record[train_size];
      uint64_t page_number[numOfLines];
 
      std::deque<ClassifiedPoint> train_save (train_size);
      //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
      std::deque<ClassifiedPoint> overall (total_size);
      train_save.clear();
      overall.clear();
 

      for (int i=0;i<total_size;i++)
      {
        ClassifiedPoint p = convertToIrisPoint_aft_2(i);
        //overall[i] = *p;
        overall.push_back(p);
        page_number[i]=overall[i].classification;
      }

      i=0;
      int match=0;
      int no_match;
      for (int z=0;z<train_size;z++)
      {
        //x=rand()%999;
        x=(1000/train_size)*(z);
        train[z]=overall[x];
        train_save.push_back(train[z]);
        record[z]=x;

      }
      while(i<total_size)
      {
        no_match=0;
        for(int j=0;j<train_size;j++)
        {
          if(i!=record[j])
          {
            no_match++;
          }
          if(i==record[j])
          {
            match++;
        //printf("Number of match is:%d\n",match);
            i++;
            break;
          }
          if(no_match==train_size)
          {
            test[i-match]=overall[i-match];
            //test.at(i-match)=overall[i-match];
            int y=i-match;
            //printf("The test iteration is:%d\n",y);
            i++;
          }
        }
      }
  //return train_save;
    }

    uint64_t findMajorityClass(std::deque<ClassifiedDistance> & neighbors,int k)
    {
      int count0=0,count1=0;
      uint64_t compare[k];
      for(int i=0;i<k;i++)
      {
        compare[i]=neighbors[i].classification;
      }
      int res=0;
      int count_major=1;
      for(int i=1;i<k;i++)
      {
        if(compare[i]==compare[res])
        {
          count_major++;
        }
        else
        {
          count_major--;
        }
        if(count_major==0)
        {
          res=i;
          count_major=1;
        }
      }
      return compare[res];
    }

    int classify_1(int dimensions, std::deque<ClassifiedPoint> & trainedData, int trainedSize, ClassifiedPoint & toClassify,int k )
    {
  
      std::deque<ClassifiedDistance> distances (trainedSize);
  
      for (int i = 0; i < trainedSize; i++)
      {
        ClassifiedDistance distance;
        distance.classification = (trainedData)[i].classification;
        distance.distance =sqrt((trainedData[i].point_1-toClassify.point_1) * (trainedData[i].point_1 - toClassify.point_1) + (trainedData[i].point_2 - toClassify.point_2) * (trainedData[i].point_2 - toClassify.point_2));
        distances[i] = distance;

      }

      //Sort the Calculated Distances
      sort(distances.begin(), distances.end(), [](ClassifiedDistance& dpa, ClassifiedDistance& dpb) -> bool
      {
        return (dpa.distance < dpb.distance);
      });

      uint64_t final_class;
      final_class=findMajorityClass(distances,k);
 
      return final_class;

    }

    void add_classification (int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size)
    {
  
      int i = 0;
      //Randomnize data
      int x =0;
      int record[train_size];

      std::deque<ClassifiedPoint> train_save (train_size);
      //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
      std::deque<ClassifiedPoint> overall (total_size);
      train_save.clear();
      overall.clear();

      for (int i=0;i<total_size;i++)
      {
          ClassifiedPoint p = convertToIrisPoint_only_page_num (i);
          //overall[i] = *p;
          overall.push_back(p);
      }

      i=0;
      int match=0;
      int no_match;
      for (int z=0;z<train_size;z++)
      {
        x=(1000/train_size)*(z);
        train[z].classification=overall[x].classification;
        record[z]=x;
      }
      while(i<total_size)
      {
        no_match=0;
        for(int j=0;j<train_size;j++)
        {
          if(i!=record[j])
          {
            no_match++;
          }
          if(i==record[j])
          {
            match++;
        //printf("Number of match is:%d\n",match);
            i++;
            break;
          }
          if(no_match==train_size)
          {
            test[i-match].classification=overall[i-match].classification;
            //test.at(i-match)=overall[i-match];
            int y=i-match;
            //printf("The test iteration is:%d\n",y);
            i++;
          }
        }
  //printf("The iteration is:%d\n",i);
      }
    } 


  };

  std::map<CACHE*, tracker> trackers;


}

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " -> Prefetcher LLC Prefetcher initialized @ " << current_cycle << " cycles." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{ /*
  if (::trackers[this].past_accesses.size() >= HISTORY_SIZE) {
    if (::trackers[this].check_within_block(::trackers[this].past_accesses.back().first, addr)) {
      ::trackers[this].past_accesses.back().second++; 
    }
    else {
      ::trackers[this].past_accesses.pop_front();
      ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
    }
  }
  else if (::trackers[this].past_accesses.size() == 0) {
    ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
  }
  else {
    if (::trackers[this].check_within_block(::trackers[this].past_accesses.back().first, addr)) {
      ::trackers[this].past_accesses.back().second++; 
    }
    else {
      ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
    }
  }
  */
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  //std::cout<<"has entered the stage"<<champsim::operable::reset_count<<std::endl;
  auto &previous_train = ::trackers[this].previous_train;
  auto &trainData = ::trackers[this].trainData;
  auto &testData = ::trackers[this].testData;
  auto &finalData = ::trackers[this].finalData;
  auto &current_train = ::trackers[this].current_train;

  if (champsim::operable::context_switch_mode)
  {
    if (!::trackers[this].context_switch_toggled)
    {
      if (!champsim::operable::have_cleared_BTB
            && !champsim::operable::have_cleared_BP
            && !champsim::operable::have_cleared_prefetcher
            && champsim::operable::cpu_side_reset_ready) 
      {
        //&& champsim::operable::cache_clear_counter == 7) {
        champsim::operable::context_switch_mode = false;
        champsim::operable::cpu_side_reset_ready = false;
        champsim::operable::cache_clear_counter = 0;
        ::trackers[this].context_switch_prefetch_gathered = false;
        ::trackers[this].context_switch_toggled = true;
        //std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycle(s)" << " done at cycle " << current_cycle << std::endl;
      }
    }
  }
    
  //std::cout << "Page prefetcher operate()" << std::endl;
  // Gather and issue prefetches after a context switch.
  if ((champsim::operable::reset_count==1)&&(champsim::operable::knn_can_predict==true))
  {
    //newly added
    std::cout<<"Enter the round 1"<<std::endl;
    uint64_t prefetch_candidate[testDataSize];
  
    ::trackers[this].trainData.assign(trainDataSize, {0});
 
    ::trackers[this].testData.assign(testDataSize, {0});
 
    ::trackers[this].finalData.assign(testDataSize, {0});

    ::trackers[this].current_train.assign(trainDataSize, {0}); 

    ::trackers[this].previous_train.assign(trainDataSize,{0});

    ::trackers[this].readTrainingData(totalDataSize, ::trackers[this].trainData,::trackers[this].testData,trainDataSize,totalDataSize);
    //current_train=trainData;
    copy(trainData.begin(), trainData.end(),current_train.begin());

    for(size_t j=0;j<testDataSize;j++)
    {
      finalData[j].classification= ::trackers[this].classify_1(2, trainData, trainDataSize, testData[j],K);
      prefetch_candidate[j]=finalData[j].classification;
    }

    float count_success=0; 
    for(size_t i=0;i<testDataSize;i++)
    {
     // std::cout<<"The estimated classification at "<<i<< "are"<<finalData[i].classification<<std::endl;
     // std::cout<<"The real classification are at "<<i<<"are"<<testData[i].classification<<std::endl;
      if(finalData[i].classification==testData[i].classification)
      {
        //printf("The classification is successful\n");
        count_success++;
      }
    }

    final_accuracy=count_success/testDataSize;
   // std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<final_accuracy<<std::endl;
    for(size_t i=0;i<trainDataSize;i++)
    {
     // std::cout<<"The traindata size is"<<current_train[i].classification<<std::endl;
    }
    //previous_train=current_train;
    copy(current_train.begin(),current_train.end(),::trackers[this].previous_train.begin());
   // std::cout<<"We are at the point after the accuracy count"<<std::endl;
    std::vector<uint64_t> actual_prefetch;
    for(size_t i=0;i<trainDataSize;i++)
    {
     // std::cout<<"The prev train  size is"<<::trackers[this].previous_train[i].classification<<std::endl;
    }
    actual_prefetch= ::trackers[this].distinct_page (prefetch_candidate,testDataSize);
   // std::cout << "actual_prefetch = " << actual_prefetch.size() << std::endl;

    // Gather prefetches
    if (!::trackers[this].context_switch_prefetch_gathered)
    {
      this->clear_internal_PQ();
      //::trackers[this].gather_context_switch_prefetches(actual_prefetch); 
      //::trackers[this].context_switch_prefetch_gathered = true;
      //::trackers[this].context_switch_prefetching_timing.clear();
      //::trackers[this].uniq_prefetched_page_address.clear();
    }
   
    // Issue prefetches until the queue is empty.
    {
      /*std::unordered_set<uint64_t> printed_page_addresses;
      
      for(auto [addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
        if (printed_page_addresses.find(addr >> 12) == printed_page_addresses.end()) {
          
          //std::cout << "Page with base address " << std::hex << addr << " issued at cycle " << std::dec << issued_at << " received at cycle " << received_at << std::endl; 
          printed_page_addresses.insert(addr >> 12);
        }
      }*/
      //clear the value
      std::cout<<"Enter the clear stage"<<std::endl;
      actual_prefetch.clear();
      trainData.clear();
      testData.clear();
      finalData.clear();
      //previous_train.clear();
      current_train.clear();
      std::cout<<"The clear is done"<<std::endl;
      champsim::operable::knn_can_predict = false;
      /*if (!champsim::operable::have_cleared_BTB
          && !champsim::operable::have_cleared_BP
          && champsim::operable::cpu_side_reset_ready
          && !champsim::operable::have_cleared_prefetcher
          ) {//&& champsim::operable::cache_clear_counter == 7
        std::cout<<"The erasing mode is entered"<<std::endl;
        champsim::operable::context_switch_mode = false;
        champsim::operable::cpu_side_reset_ready = false;
        champsim::operable::cache_clear_counter = 0;
        //champsim::operable::knn_can_predict = false;
        ::trackers[this].context_switch_prefetch_gathered = false;
        std::cout<<"The clear mode is over"<<std::endl;
        //std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
      }*/
    }
  }

  //second round

  if (champsim::operable::context_switch_mode && (champsim::operable::reset_count>1))
  {
    //newly added
    uint64_t prefetch_candidate[testDataSize];
  
    // std::deque<ClassifiedPoint> trainData (trainDataSize);
 
    // std::deque<ClassifiedPoint> testData (testDataSize);
 
    // std::deque<ClassifiedPoint> finalData (testDataSize);

    // std::deque<ClassifiedPoint> current_train(trainDataSize); 

    //::trackers[this].readTrainingData_aft_2(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
    //current_train=trainData;

    //for(size_t j=0;j<testDataSize;j++)
    //{
      //std::cout<<"The loop is entered"<<std::endl;
      //finalData[j].classification= ::trackers[this].classify_1(2,::trackers[this].previous_train, trainDataSize, testData[j],K);
      //prefetch_candidate[j]=finalData[j].classification;
    //}

   
    std::vector<uint64_t> actual_prefetch;
    //actual_prefetch= ::trackers[this].distinct_page (prefetch_candidate,testDataSize);
    //std::cout << "actual_prefetch = " << actual_prefetch.size() << std::endl;

    // Gather prefetches
    if (!::trackers[this].context_switch_prefetch_gathered)
    {
      //newly added
      std::cout<<"Enter the round above 1"<<std::endl;
      ::trackers[this].readTrainingData_aft_2(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
      for(size_t j=0;j<testDataSize;j++)
      {
        //std::cout<<"The loop is entered"<<std::endl;
        finalData[j].classification= ::trackers[this].classify_1(2,::trackers[this].previous_train, trainDataSize, testData[j],K);
        prefetch_candidate[j]=finalData[j].classification;
      }
      actual_prefetch= ::trackers[this].distinct_page (prefetch_candidate,testDataSize);
      std::cout << "actual_prefetch = " << actual_prefetch.size() << std::endl;

      this->clear_internal_PQ();
      ::trackers[this].gather_context_switch_prefetches(actual_prefetch); 
      ::trackers[this].context_switch_prefetch_gathered = true;
      //::trackers[this].context_switch_prefetching_timing.clear();
      //::trackers[this].uniq_prefetched_page_address.clear();
    }
   
    // Issue prefetches until the queue is empty.
    if (!::trackers[this].context_switch_queue_empty())
    {
      if (champsim::operable::cpu_side_reset_ready) {
       ::trackers[this].context_switch_issue(this);

       
        /*for(auto &[addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
          if (received_at == 0) {
            for(auto var : block) {
              if (var.valid && var.address == addr) {
                received_at = this->current_cycle; 
              }
            } 
          } 
        }*/
      }
    }
    // Toggle switches after all prefetches are issued.
    else
    {
      /*std::unordered_set<uint64_t> printed_page_addresses;
      
      for(auto [addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
        if (printed_page_addresses.find(addr >> 12) == printed_page_addresses.end()) {
          
          //std::cout << "Page with base address " << std::hex << addr << " issued at cycle " << std::dec << issued_at << " received at cycle " << received_at << std::endl; 
          printed_page_addresses.insert(addr >> 12);
        }
      }*/

        if (!champsim::operable::have_cleared_BTB
          && !champsim::operable::have_cleared_BP
          && champsim::operable::cpu_side_reset_ready
          && !champsim::operable::have_cleared_prefetcher
          ) {//&& champsim::operable::cache_clear_counter == 7
        champsim::operable::context_switch_mode = false;
        champsim::operable::cpu_side_reset_ready = false;
        champsim::operable::cache_clear_counter = 0;
        //champsim::operable::knn_can_predict = false;
        ::trackers[this].context_switch_prefetch_gathered = false;
        actual_prefetch.clear();
        //std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
        }

    }
  }
  else
  {
    //add classification
    if(champsim::operable::knn_can_predict==true)
    {
      ::trackers[this].add_classification(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
      copy(trainData.begin(), trainData.end(),current_train.begin());
      float count_success=0; 
      for(size_t i=0;i<testDataSize;i++)
      {
        if(finalData[i].classification==testData[i].classification)
        {
          count_success++;
        }
      }

      final_accuracy=count_success/testDataSize;
      std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<final_accuracy<<std::endl;
      if(final_accuracy<0.6)
      {
        //previous_train=current_train;
        copy(current_train.begin(),current_train.end(),trackers[this].previous_train.begin());
        std::cout<<"Replacement at round "<<(champsim::operable::reset_count)<<std::endl;
      }
      else
      {
        std::cout<<"No Replacement at round,which is"<<(champsim::operable::reset_count)<<std::endl;
      }

      champsim::operable::knn_can_predict=false;
      //clear the value     
      //actual_prefetch.clear();
      trainData.clear();
      testData.clear();
      finalData.clear();
      //previous_train.clear();
      current_train.clear();

    }
  }
}

void CACHE::prefetcher_final_stats() {}

// WL 
