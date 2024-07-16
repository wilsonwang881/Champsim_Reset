#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <vector>
#include <deque>
//#include <operable.h>
#include "cache.h"

#define trainDataSize 25
#define testDataSize 975
#define totalDataSize 1000
#define K 3


//#define ON_DEMAND_ACCESS_RECORD_SIZE 1000
//void CACHE::prefetcher_initialize() {}
//std::deque<ClassifiedPoint> previous_train (trainDataSize);
uint64_t champsim::operable::knn_accuracy=0;
namespace
{

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

//typedef struct {
  //u_int64_t cycle;
  //u_int64_t ip;
//} on_demand_ins_access;

//on_demand_ins_access before_reset_on_demand_ins_access [ON_DEMAND_ACCESS_RECORD_SIZE];
//on_demand_ins_access after_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];

//ClassifiedPoint*train_save;

int cmpfunc (const void*a,const void*b)
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
    std::cout<<"The special pages are"<<arr[i]<<std::endl;
  }
  return special_page;
   //printf("The number of distinct page is:%d\n",special_page);
}
/**
 * Calculating the Euclidean Distance
 * 
 * √((p1_1 - p2_1)^2 + (p1_2 - p2_2)^2 + ... + (p1_x - p2_x)^2))
 * 
 * Input Parameters: 
 *  dimensions: Count of Dimensions the Euklid should work
 *  point1: Array of float coordinates
 *  point2: Array of float coordinates
 * 
 *   * 
 */
uint64_t eukl_distance(int dimensions,uint64_t* point1, uint64_t* point2){//calculate the distance between the points
  uint64_t temp = 0;
  for(int i = 0; i < dimensions; i++){
     //std::cout<<"Inside eukl, The eukl is started"<<std::endl;
     //std::cout<<"The point at"<<i<<"is"<<point1[i]<<std::endl;
     //std::cout<<"The point at"<<i<<"is"<<point2[i]<<std::endl;
     temp += pow(point1[i] - point2[i], 2.0);
     //std::cout<<"The distance is"<<temp<<std::endl;
     //std::cout<<"Inside eukl, The eukl is finished up"<<std::endl;
  }
  //std::cout<<"The distance is"<<sqrt(temp)<<std::endl;
  return sqrt(temp);
}

/**
 * Comparator Function to Compare 2 Classified Distances by their Distance
 */
int compareCdistances(const void* a, const void* b){
  ClassifiedDistance d1 = *((ClassifiedDistance*) a);
  ClassifiedDistance d2 = *((ClassifiedDistance*) b);

  if(d1.distance == d2.distance) return 0;
  else if(d1.distance < d2.distance) return -1;
  else return 1;
}

/**
 * Sorting Function to Sort Array of ClassifiedDistances
 */
void sortCD(ClassifiedDistance* data, int size){
  qsort(data, size, sizeof(ClassifiedDistance), compareCdistances);
}

/**
 * Classification function K-Nearest-Neighbour
 * 
 * Input Parameters:
 *  dimensions: count of Dimensions the Knn works with
 *  trainedData: ClassifiedPoint Array of the Trained Data
 *  trainedSize: Size of the Trained Array
 *  toClassify: Float Array that represents the Point to be classified
 */

/**
 * Convert Line of CSV File to Classified Point
 * (Works with the Iris.csv format)
 * 
 * Iris.csv:  petalLength,petalWidth,sepalLength,sepalWidth,target(in int)
 * 
 * Input Parameters:
 *  inp: char* buffer wirh line of the file
 */
ClassifiedPoint convertToIrisPoint(int x){
  //ClassifiedPoint* point = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint));
  ClassifiedPoint point {};
  //uint64_t * pos = (uint64_t *)malloc(2 * sizeof(uint64_t));
  uint64_t pos[2];
  //before clk
  pos[0] = reset_misc::before_reset_on_demand_ins_access[x].cycle;
  //pos[0] = reset_misc::before_reset_on_demand_data_access[x].cycle;
  //before adr
  pos[1] = reset_misc::before_reset_on_demand_ins_access[x].ip;
  //pos[1] = reset_misc::before_reset_on_demand_data_access[x].ip;
  //Septal Length
  //pos[2] = strtof(strtok(NULL, ","),NULL);
  //Septal Width
  //pos[3] = strtof(strtok(NULL, ","),NULL);
  
  point.point_1= pos[0];
  point.point_2 =pos[1];
  //Target
  uint64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
  //uint64_t page_num= (reset_misc::after_reset_on_demand_data_access[x].ip)>>12;
  point.classification = page_num;

  return point;
}

ClassifiedPoint convertToIrisPoint_aft_2 (int x){
  //ClassifiedPoint* point = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint));
  ClassifiedPoint point {};
  //uint64_t * pos = (uint64_t *)malloc(2 * sizeof(uint64_t));
  uint64_t pos[2];
  //before clk
  pos[0] = reset_misc::before_reset_on_demand_ins_access[x].cycle;
  //pos[0] = reset_misc::before_reset_on_demand_data_access[x].cycle;
  //before adr
  pos[1] = reset_misc::before_reset_on_demand_ins_access[x].ip;
  //pos[1] = reset_misc::before_reset_on_demand_data_access[x].ip;
  //Septal Length
  //pos[2] = strtof(strtok(NULL, ","),NULL);
  //Septal Width
  //pos[3] = strtof(strtok(NULL, ","),NULL);
  
  point.point_1= pos[0];
  point.point_2 =pos[1];
  //Target
  //uint64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
  //uint64_t page_num= (reset_misc::after_reset_on_demand_data_access[x].ip)>>12;
  //point.classification = page_num;

  return point;
}

ClassifiedPoint convertToIrisPoint_only_page_num (int x){
  //ClassifiedPoint* point = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint));
  ClassifiedPoint point {};
  
  //Target
  uint64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
  //uint64_t page_num= (reset_misc::after_reset_on_demand_data_access[x].ip)>>12;
  point.classification = page_num;

  return point;
}

/**
 * Reading Data from the specified File
 * 
 * Input Parameter:
 *  filename: Path to File
 *  numOfLines: Number of Lines to Read
 *  ret: Array the Points schould be saved to
 */
  void readTrainingData(int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size){
  //FILE* fileptr;
  //fileptr = fopen(filename, "r");
  //fseek(fileptr,saved,SEEK_SET);

  //int bufferLen = 255;
  //char buffer[bufferLen];
  int i = 0;
  //Randomnize data
  int x =0;
  int record[train_size];
  uint64_t page_number[numOfLines];
  //allocate the space for determining the KNN label
  //point_label* label = malloc(train_size*sizeof(point_label));

  //ClassifiedPoint*train_save= (ClassifiedPoint *)malloc(sizeof(ClassifiedPoint) * train_size);
  std::deque<ClassifiedPoint> train_save (train_size);
  //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
  std::deque<ClassifiedPoint> overall (total_size);
  train_save.clear();
  overall.clear();
  //train.clear();
  //test.clear();
  //if(overall==NULL)
    //std::cout<<"The allocation failure occurs"<<std::endl;

  for (int i=0;i<total_size;i++){
      ClassifiedPoint p = convertToIrisPoint(i);
      //overall[i] = *p;
      overall.push_back(p);
      //std::cout<<"The overall should have the point value at "<<i<<"is"<<overall[i].classification<<std::endl;
      //std::cout<<"The overall should have the point value at "<<i<<"is"<<overall[i]->point<<std::endl;
      page_number[i]=overall[i].classification;
      //printf("Has entered\n");
  }
  //fseek(fileptr,0,SEEK_CUR);
  //saved=ftell(fileptr);
  //printf("The file pointer after the reading is: %ld \n",ftell(fileptr));

  //find the unique pages
  //distinct_page(page_number,numOfLines,special_page,page_number);
  //get the training data
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
    //printf("The training data set is: %d\n",train_save[z].classification);
    //printf("The record number is: %d\n",record[z]);
    //std::cout<<"The train_save has"<<train_save[z].classification<<std::endl;
    //std::cout<<"The train_save has the point value at "<<z<<"is"<<train_save[z].point[0]<<std::endl;
    //std::cout<<"The train_save has the point value "<<z<<"is"<<train_save[z].point[1]<<std::endl;
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
  //printf("The iteration is:%d\n",i);
  }
  //fclose(fileptr);
  //std::cout<<"The lpoop is out"<<std::endl;
  for(int i=0;i<train_save.size();i++)
  {
    //std::cout<<"The train_saved elements are "<<i<<"at"<<train_save[i].classification<<std::endl;
  }
  //return train_save;
}

//uint64_t* compare (const void * a, const void * b)
//{
  //return ( *(uint64_t*)a - *(uint64_t*)b );
//}

void readTrainingData_aft_2 (int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size){
  //FILE* fileptr;
  //fileptr = fopen(filename, "r");
  //fseek(fileptr,saved,SEEK_SET);

  //int bufferLen = 255;
  //char buffer[bufferLen];
  int i = 0;
  //Randomnize data
  int x =0;
  int record[train_size];
  //uint64_t page_number[numOfLines];
  //allocate the space for determining the KNN label
  //point_label* label = malloc(train_size*sizeof(point_label));

  //ClassifiedPoint*train_save= (ClassifiedPoint *)malloc(sizeof(ClassifiedPoint) * train_size);
  std::deque<ClassifiedPoint> train_save (train_size);
  //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
  std::deque<ClassifiedPoint> overall (total_size);
  train_save.clear();
  overall.clear();
  //train.clear();
  //test.clear();
  //if(overall==NULL)
    //std::cout<<"The allocation failure occurs"<<std::endl;

    for (int i=0;i<total_size;i++){
      ClassifiedPoint p =  convertToIrisPoint_aft_2(i);
      //overall[i] = *p;
      overall.push_back(p);
      //std::cout<<"The overall should have the point value at "<<i<<"is"<<overall[i].classification<<std::endl;
      //std::cout<<"The overall should have the point value at "<<i<<"is"<<overall[i]->point<<std::endl;
      //page_number[i]=overall[i].classification;
      //printf("Has entered\n");
  }
  i=0;
  int match=0;
  int no_match;
  for (int z=0;z<train_size;z++)
  {
    x=(1000/train_size)*(z);
    train[z]=overall[x];
    train_save.push_back(train[z]);
    record[z]=x;
    //printf("The training data set is: %d\n",train_save[z].classification);
    //printf("The record number is: %d\n",record[z]);
    //std::cout<<"The train_save has"<<train_save[z].classification<<std::endl;
    //std::cout<<"The train_save has the point value at "<<z<<"is"<<train_save[z].point[0]<<std::endl;
    //std::cout<<"The train_save has the point value "<<z<<"is"<<train_save[z].point[1]<<std::endl;
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
  //printf("The iteration is:%d\n",i);
  }
  //fclose(fileptr);
  //std::cout<<"The lpoop is out"<<std::endl;
  //return train_save;
}


uint64_t findMajorityClass(std::deque<ClassifiedDistance> & neighbors,int k){
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

int classify_1(int dimensions, std::deque<ClassifiedPoint> & trainedData, int trainedSize, ClassifiedPoint & toClassify,int k ){
  
  //printf("The testing point is: %f %f\n",toClassify[0],toClassify[1]);
  //ClassifiedDistance * distances = (ClassifiedDistance *) malloc(sizeof(ClassifiedDistance) * trainedSize);
  std::deque<ClassifiedDistance> distances (trainedSize);
  //std::cout<<"Inside Classify_1, The malloc is finished up"<<std::endl;
  //Calculating the Euklid distance to every trained Datapoint
  for (int i = 0; i < trainedSize; i++)
  {
    ClassifiedDistance distance;
    //std::cout<<"Inside Classify_1, The classified distance isfinished up"<<std::endl;
    distance.classification = (trainedData)[i].classification;
    //std::cout<<"Inside Classify_1, The classification is finished up"<<std::endl;
    //distance.distance = eukl_distance(dimensions, (trainedData)[i].point, toClassify);
    distance.distance =sqrt((trainedData[i].point_1-toClassify.point_1) * (trainedData[i].point_1 - toClassify.point_1) + (trainedData[i].point_2 - toClassify.point_2) * (trainedData[i].point_2 - toClassify.point_2));
    //std::cout<<"Inside Classify_1, The eukl is finished up"<<std::endl;
    distances[i] = distance;
    //std::cout<<"distance at"<<i<< "is"<< distances[i].distance<<std::endl;
    //std::cout<<"The points are 1"<<trainedData[i].point_1<<" at 2"<<trainedData[i].point_2<<std::endl;
  }

  //Sort the Calculated Distances
  //sortCD(distances, trainedSize);
  sort(distances.begin(), distances.end(), [](ClassifiedDistance& dpa, ClassifiedDistance& dpb) -> bool
  {
        return (dpa.distance < dpb.distance);
  });
  //for(int i=0;i<trainedSize;i++)
  //{
    //std::cout<<"distance at"<<i<< "is"<< distances[i].distance<<"and classification is"<<distances[i].classification<<std::endl;
  //}
  uint64_t final_class;
  final_class=findMajorityClass(distances,k);
  //printf("The final classification class is:%d\n",final_class);
  //std::cout<<"The final class is: "<<final_class<<std::endl;
  return final_class;

}

void add_classification (int numOfLines, std::deque<ClassifiedPoint> &train, std::deque<ClassifiedPoint> &test, int train_size ,int total_size){
  //FILE* fileptr;
  //fileptr = fopen(filename, "r");
  //fseek(fileptr,saved,SEEK_SET);

  //int bufferLen = 255;
  //char buffer[bufferLen];
  int i = 0;
  //Randomnize data
  int x =0;
  int record[train_size];

  //ClassifiedPoint*train_save= (ClassifiedPoint *)malloc(sizeof(ClassifiedPoint) * train_size);
  std::deque<ClassifiedPoint> train_save (train_size);
  //ClassifiedPoint*overall = (ClassifiedPoint *) malloc(sizeof(ClassifiedPoint) * 1000);
  std::deque<ClassifiedPoint> overall (total_size);
  train_save.clear();
  overall.clear();
  //train.clear();
  //test.clear();
  //if(overall==NULL)
    //std::cout<<"The allocation failure occurs"<<std::endl;

  for (int i=0;i<total_size;i++){
      ClassifiedPoint p = convertToIrisPoint_only_page_num (i);
      //overall[i] = *p;
      overall.push_back(p);
  }
  //fseek(fileptr,0,SEEK_CUR);
  //saved=ftell(fileptr);
  //printf("The file pointer after the reading is: %ld \n",ftell(fileptr));

  //find the unique pages
  //distinct_page(page_number,numOfLines,special_page,page_number);
  //get the training data
  i=0;
  int match=0;
  int no_match;
  for (int z=0;z<train_size;z++)
  {
    x=(1000/train_size)*(z);
    train[z].classification=overall[x].classification;
    //train_save.push_back(train[z]);
    record[z]=x;
    //printf("The training data set is: %d\n",train_save[z].classification);
    //printf("The record number is: %d\n",record[z]);
    //std::cout<<"The train_save has"<<train_save[z].classification<<std::endl;
    //std::cout<<"The train_save has the point value at "<<z<<"is"<<train_save[z].point[0]<<std::endl;
    //std::cout<<"The train_save has the point value "<<z<<"is"<<train_save[z].point[1]<<std::endl;
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
  //fclose(fileptr);
  //std::cout<<"The lpoop is out"<<std::endl;
  //return train_save;
}


}
void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  std::deque<ClassifiedPoint> previous_train (trainDataSize);
  if(champsim::operable::reset_count>0)
  {
    //std::cout<<"at round"<<champsim::operable::reset_count<<std::endl;
  }
  if(champsim::operable::reset_count==1)
  {
  std::cout<<"Enter the round 1"<<std::endl;
  uint64_t prefetch_candidate[testDataSize];
  //ClassifiedPoint *  trainData = (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint) * trainDataSize);
  std::deque<ClassifiedPoint> trainData (trainDataSize);
  //ClassifiedPoint * testData= (ClassifiedPoint*)malloc (sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> testData (testDataSize);
  //ClassifiedPoint * finalData= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> finalData (testDataSize);
  //ClassifiedPoint * previous_train= (ClassifiedPoint*) malloc(sizeof(ClassifiedPoint)* trainDataSize);
  //std::deque<ClassifiedPoint> previous_train (trainDataSize);
  //ClassifiedPoint * current_train= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* trainDataSize);
  std::deque<ClassifiedPoint> current_train(trainDataSize); 
  //std::cout<<"The core malloc is finished up"<<std::endl;
  //Read the Trained Data from CSV- File and Store it into TrainData Array
  readTrainingData(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
  //current_train=trainData;
  copy(trainData.begin(), trainData.end(),
        current_train.begin());
  //std::cout<<"The size of the current_train elements is"<<current_train.size()<<std::endl;
  //for(int i=0;i<15;i++)
 // {
    //std::cout<<"The current_train elements are "<<i<<"at"<<current_train[i].classification<<std::endl;
  //}
  //Finding the after page number
  for(size_t j=0;j<testDataSize;j++)
  {
    //std::cout<<"The loop is entered"<<std::endl;
    finalData[j].classification= classify_1(2, trainData, trainDataSize, testData[j],K);
    //std::cout<<"at"<<j<<"The current_train points are"<<trainData[j].point_1<<"and"<<trainData[j].point_2<<std::endl;
    //std::cout<<"at"<<j<<"The testData points are"<<testData[j].point_1<<"and"<<testData[j].point_2<<std::endl;
    prefetch_candidate[j]=finalData[j].classification;
    //std::cout<<"The classification are"<<finalData[j].classification<<std::endl;
    //for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    //{
      //uint64_t pf_addr = ((finalData[j].classification)<<LOG2_PAGE_SIZE)+x*64;
      //prefetch_line(pf_addr, true, metadata_in);
    //}
  }
  //std::cout<<"The prefetch is finished up"<<std::endl;
  //accuracy check
  float count_success=0; 
  for(size_t i=0;i<testDataSize;i++)
  {
    std::cout<<"The estimated classification at "<<i<< "are"<<finalData[i].classification<<std::endl;
    std::cout<<"The real classification are at "<<i<<"are"<<testData[i].classification<<std::endl;
    if(finalData[i].classification==testData[i].classification)
    {
      //printf("The classification is successful\n");
      count_success++;
    }
  }

  final_accuracy=count_success/testDataSize;
  //printf("The count_success is:%f\n",count_success);
  //printf("The testDataSize is:%d\n",testDataSize);
  //printf("The final accuracy is:%f \n",final_accuracy);
  std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<final_accuracy<<std::endl;
  //previous_train=current_train;
  copy(current_train.begin(),current_train.end(),previous_train.begin());
  std::vector<uint64_t> actual_prefetch;
  actual_prefetch= distinct_page (prefetch_candidate,testDataSize);
  std::cout << "actual_prefetch = " << actual_prefetch.size() << std::endl;
  for(int i=0;i<actual_prefetch.size();i++)
  {
    for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    {
      //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
      uint64_t pf_addr = (actual_prefetch[i]<<LOG2_PAGE_SIZE)+x*64;
      prefetch_line(pf_addr, true, metadata_in);
      /*
      if (!prefetch_line(pf_addr, true, metadata_in)){
        //x--;
        //std::cout<<"It is full "<<"The address is"<<pf_addr<<"at"<<x<<std::endl;
      }
      */
    }
  }
  //if(actual_prefetch.size()!=1)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
  //}
  //for(size_t i=0;i<actual_prefetch.size();i++)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
    //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
  //}
  actual_prefetch.clear();
  trainData.clear();
  testData.clear();
  finalData.clear();
  //previous_train.clear();
  current_train.clear();

  //accuracy counting

  //uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
  //prefetch_line(pf_addr, true, metadata_in);
  //return metadata_in;
  champsim::operable::knn_can_predict == 0 ;
  }

  //after the 1st round no replacement



  if((champsim::operable::knn_can_predict == true) && (champsim::operable::reset_count>1))
  {
  //std::cout<<"No Replacement at round"<<champsim::operable::reset_count<<std::endl;
  uint64_t prefetch_candidate[testDataSize];
  //ClassifiedPoint *  trainData = (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint) * trainDataSize);
  std::deque<ClassifiedPoint> trainData (trainDataSize);
  //ClassifiedPoint * testData= (ClassifiedPoint*)malloc (sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> testData (testDataSize);
  //ClassifiedPoint * finalData= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> finalData (testDataSize);
  //ClassifiedPoint * previous_train= (ClassifiedPoint*) malloc(sizeof(ClassifiedPoint)* trainDataSize);
  //std::deque<ClassifiedPoint> previous_train (trainDataSize);
  //ClassifiedPoint * current_train= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* trainDataSize);
  std::deque<ClassifiedPoint> current_train(trainDataSize); 
  //std::cout<<"The core malloc is finished up"<<std::endl;
  //Read the Trained Data from CSV- File and Store it into TrainData Array
  readTrainingData_aft_2(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
  //current_train=trainData;
  //std::cout<<"The size of the current_train elements is"<<current_train.size()<<std::endl;
  //for(int i=0;i<15;i++)
 // {
    //std::cout<<"The current_train elements are "<<i<<"at"<<current_train[i].classification<<std::endl;
  //}
  //Finding the after page number
  for(size_t j=0;j<testDataSize;j++)
  {
    //std::cout<<"The loop is entered"<<std::endl;
    finalData[j].classification= classify_1(2, previous_train, trainDataSize, testData[j],K);
    //std::cout<<"at"<<j<<"The current_train points are"<<trainData[j].point_1<<"and"<<trainData[j].point_2<<std::endl;
    //std::cout<<"at"<<j<<"The testData points are"<<testData[j].point_1<<"and"<<testData[j].point_2<<std::endl;
    prefetch_candidate[j]=finalData[j].classification;
    //std::cout<<"The classification are"<<finalData[j].classification<<std::endl;
    //for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    //{
      //uint64_t pf_addr = ((finalData[j].classification)<<LOG2_PAGE_SIZE)+x*64;
      //prefetch_line(pf_addr, true, metadata_in);
    //}
  }
  //start prefetch
  std::vector<uint64_t> actual_prefetch;
  actual_prefetch= distinct_page (prefetch_candidate,testDataSize);
  for(int i=0;i<actual_prefetch.size();i++)
  {
    for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    {
      //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
      uint64_t pf_addr = (actual_prefetch[i]<<LOG2_PAGE_SIZE)+x*64;
      prefetch_line(pf_addr, true, metadata_in);
    }
  }
  //accuracy check
  add_classification(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
  //current_train=trainData;
  copy(trainData.begin(), trainData.end(),
         current_train.begin());
  float count_success=0; 
  for(size_t i=0;i<testDataSize;i++)
  {
    //std::cout<<"The estimated classification at "<<i<< "are"<<finalData[i].classification<<std::endl;
    //std::cout<<"The real classification are at "<<i<<"are"<<testData[i].classification<<std::endl;
    if(finalData[i].classification==testData[i].classification)
    {
      //printf("The classification is successful\n");
      //std::cout<<"The classification is successful at"<<testData[i].classification<<std::endl;
      count_success++;
    }
  }

  //champsim::operable::knn_accuracy=count_success/testDataSize;
  final_accuracy=count_success/testDataSize;
  //printf("The count_success is:%f\n",count_success);
  //printf("The testDataSize is:%d\n",testDataSize);
  std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<final_accuracy<<std::endl;
  if(final_accuracy<0.6)
  {
    //previous_train=current_train;
    copy(current_train.begin(),current_train.end(),
         previous_train.begin());
    std::cout<<"Replacement at round "<<(champsim::operable::reset_count)<<std::endl;
  }
  else
  {
     std::cout<<"No Replacement at round,which is"<<(champsim::operable::reset_count)<<std::endl;
  }

  //if(actual_prefetch.size()!=1)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
  //}
  //for(size_t i=0;i<actual_prefetch.size();i++)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
    //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
  //}
  actual_prefetch.clear();
  trainData.clear();
  testData.clear();
  finalData.clear();
  //previous_train.clear();
  current_train.clear();

  //accuracy counting

  //uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
  //prefetch_line(pf_addr, true, metadata_in);
  //return metadata_in;
  champsim::operable::knn_can_predict == 0 ;
  }

/*
  //IF we meed the replacement 
  if((champsim::operable::knn_can_predict == true) && (champsim::operable::reset_count>1)&&(final_accuracy<=0.7))
  {
  //std::cout<<"Has Replacement at round"<<champsim::operable::reset_count<<std::endl;
  uint64_t prefetch_candidate[testDataSize];
  //ClassifiedPoint *  trainData = (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint) * trainDataSize);
  std::deque<ClassifiedPoint> trainData (trainDataSize);
  //ClassifiedPoint * testData= (ClassifiedPoint*)malloc (sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> testData (testDataSize);
  //ClassifiedPoint * finalData= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* testDataSize);
  std::deque<ClassifiedPoint> finalData (testDataSize);
  //ClassifiedPoint * previous_train= (ClassifiedPoint*) malloc(sizeof(ClassifiedPoint)* trainDataSize);
  //std::deque<ClassifiedPoint> previous_train (trainDataSize);
  //ClassifiedPoint * current_train= (ClassifiedPoint*)malloc(sizeof(ClassifiedPoint)* trainDataSize);
  std::deque<ClassifiedPoint> current_train(trainDataSize); 
  //std::cout<<"The core malloc is finished up"<<std::endl;
  //Read the Trained Data from CSV- File and Store it into TrainData Array
  readTrainingData_aft_2(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
  //current_train=trainData;
  //std::cout<<"The size of the current_train elements is"<<current_train.size()<<std::endl;
  //for(int i=0;i<15;i++)
 // {
    //std::cout<<"The current_train elements are "<<i<<"at"<<current_train[i].classification<<std::endl;
  //}
  //Finding the after page number
  for(size_t j=0;j<testDataSize;j++)
  {
    //std::cout<<"The loop is entered"<<std::endl;
    finalData[j].classification= classify_1(2, previous_train, trainDataSize, testData[j],K);
    //std::cout<<"at"<<j<<"The current_train points are"<<trainData[j].point_1<<"and"<<trainData[j].point_2<<std::endl;
    //std::cout<<"at"<<j<<"The testData points are"<<testData[j].point_1<<"and"<<testData[j].point_2<<std::endl;
    prefetch_candidate[j]=finalData[j].classification;
    //std::cout<<"The classification are"<<finalData[j].classification<<std::endl;
    //for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    //{
      //uint64_t pf_addr = ((finalData[j].classification)<<LOG2_PAGE_SIZE)+x*64;
      //prefetch_line(pf_addr, true, metadata_in);
    //}
  }
  //std::cout<<"The prefetch is finished up"<<std::endl;
  //accuracy check
  float count_success=0; 
  for(size_t i=0;i<testDataSize;i++)
  {
    //std::cout<<"The estimated classification at "<<i<< "are"<<finalData[i].classification<<std::endl;
    //std::cout<<"The real classification are at "<<i<<"are"<<testData[i].classification<<std::endl;
    if(finalData[i].classification==testData[i].classification)
    {
      //printf("The classification is successful\n");
      count_success++;
    }
  }

  //champsim::operable::knn_accuracy=count_success/testDataSize;
  final_accuracy=count_success/testDataSize;
  //printf("The count_success is:%f\n",count_success);
  //printf("The testDataSize is:%d\n",testDataSize);
  //std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<champsim::operable::knn_accuracy<<std::endl;
  std::cout<<"The accuracy at round"<<champsim::operable::reset_count<<"is"<<final_accuracy<<std::endl;
  previous_train=current_train;

  std::vector<uint64_t> actual_prefetch;
  actual_prefetch= distinct_page (prefetch_candidate,testDataSize);
  for(int i=0;i<actual_prefetch.size();i++)
  {
    for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    {
      //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
      uint64_t pf_addr = (actual_prefetch[i]<<LOG2_PAGE_SIZE)+x*64;
      prefetch_line(pf_addr, true, metadata_in);
    }
  }
  //if(actual_prefetch.size()!=1)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
  //}
  //for(size_t i=0;i<actual_prefetch.size();i++)
  //{
    //std::cout<<"The actual size of the prefetch is"<< actual_prefetch.size()<<std::endl;
    //std::cout<<"The page we tend to prefetch is"<< actual_prefetch[i]<<std::endl;
  //}
  actual_prefetch.clear();
  trainData.clear();
  testData.clear();
  finalData.clear();
  //previous_train.clear();
  current_train.clear();

  //accuracy counting

  //uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
  //prefetch_line(pf_addr, true, metadata_in);
  //return metadata_in;
  champsim::operable::knn_can_predict == 0 ;
  }  
*/

}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
