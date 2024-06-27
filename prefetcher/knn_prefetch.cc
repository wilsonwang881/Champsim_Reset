#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
//#include <vector>
#include <operable.h>
#include "cache.h"

int trainDataSize=15;
int testDataSize=985;
int totalDataSize=1000;
int K=3;


//#define ON_DEMAND_ACCESS_RECORD_SIZE 1000

ClassifiedPoint * previous_train= malloc(sizeof(ClassifiedPoint)* trainDataSize);
float final_accuracy=0;

long saved=0;
int special_page=0;
int no_replace=0;
int *store_page[20]={0};

typedef struct{
  u_int64_t * point;
  u_int64_t classification;
} ClassifiedPoint;

typedef struct{
  u_int64_t classification;
  u_int64_t distance;
} ClassifiedDistance;

//typedef struct {
  //u_int64_t cycle;
  //u_int64_t ip;
//} on_demand_ins_access;

//on_demand_ins_access before_reset_on_demand_ins_access [ON_DEMAND_ACCESS_RECORD_SIZE];
//on_demand_ins_access after_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];

ClassifiedPoint*train_save;

int cmpfunc (const void*a,const void*b)
{
  return(*(int*)a-*(int*)b);
}

void distinct_page (int arr[],int n,int special_page,int*array[1000]) //finding distinct pages for every 1000 points
{
  qsort(arr,n,sizeof(int),cmpfunc);
  for(int i=0;i<n;i++)
  {
    while (i<n-1 && arr[i]==arr[i+1])
    {
      i++;
    }
    if(arr[i]!=0)
    {
      special_page++;
      //printf("The pages are:%d\n",arr[i]);
      //array[special_page]=&arr[i];
      //printf("The pages are:%d\n",*array[special_page]);
    }
  }
  printf("The number of distinct page is:%d\n",special_page);
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
u_int64_t eukl_distance(int dimensions,u_int64_t* point1, u_int64_t* point2){//calculate the distance between the points
  double temp = 0;
  for(int i = 0; i < dimensions; i++){
     temp += pow(point1[i] - point2[i], 2.0);
  }
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

ClassifiedDistance* classify(int dimensions, const ClassifiedPoint* trainedData, int trainedSize, u_int64_t* toClassify){

  //printf("Enters the classification\n");
  ClassifiedDistance * distances = malloc(sizeof(ClassifiedDistance) * trainedSize); //assign the space for the training point

  //Calculating the Euklid distance to every trained Datapoint
  for (int i = 0; i < trainedSize; i++)
  {
    ClassifiedDistance distance;
    distance.classification = (trainedData)[i].classification;
    distance.distance = eukl_distance(dimensions, (trainedData)[i].point, toClassify);
    distances[i] = distance;
    //printf("The distance is at %d: %f\n",i,distances[i].distance);
  }

  //Sort the Calculated Distances
  sortCD(distances, trainedSize);
  //printf("Finish up the classification \n");
  //printf("The final classification is:%d\n",distances[0].classification);
  return distances;
}



/**
 * Convert Line of CSV File to Classified Point
 * (Works with the Iris.csv format)
 * 
 * Iris.csv:  petalLength,petalWidth,sepalLength,sepalWidth,target(in int)
 * 
 * Input Parameters:
 *  inp: char* buffer wirh line of the file
 */
ClassifiedPoint* convertToIrisPoint(int x){
  ClassifiedPoint* point = malloc(sizeof(ClassifiedPoint));
  u_int64_t * pos = malloc(2 * sizeof(u_int64_t));
  //before clk
  pos[0] = reset_misc::before_reset_on_demand_ins_access[x].cycle;
  //before adr
  pos[1] = reset_misc::before_reset_on_demand_ins_access[x].ip;
  //Septal Length
  //pos[2] = strtof(strtok(NULL, ","),NULL);
  //Septal Width
  //pos[3] = strtof(strtok(NULL, ","),NULL);
  
  point->point = pos;
  //Target
  u_int64_t page_num= (reset_misc::after_reset_on_demand_ins_access[x].ip)>>12;
  point->classification = page_num;
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
ClassifiedPoint*readTrainingData(int numOfLines, ClassifiedPoint* train, ClassifiedPoint* test, int train_size,int total_size){
  //FILE* fileptr;

  //fileptr = fopen(filename, "r");
  //fseek(fileptr,saved,SEEK_SET);

  //int bufferLen = 255;
  //char buffer[bufferLen];
    int i = 0;
  //Randomnize data
  int x =0;
  int record[train_size];
  u_int64_t page_number[numOfLines];
  //allocate the space for determining the KNN label
  //point_label* label = malloc(train_size*sizeof(point_label));
  ClassifiedPoint*train_save= malloc(sizeof(ClassifiedPoint) * train_size);

  ClassifiedPoint*overall = malloc(sizeof(ClassifiedPoint) * ON_DEMAND_ACCESS_RECORD_SIZE);

    for (int i=0;i<ON_DEMAND_ACCESS_RECORD_SIZE;i++){
      ClassifiedPoint* p =  convertToIrisPoint(i);
      overall[i] = *p;
      page_number[i]=overall[i].classification;
      //printf("Has entered\n");
      i++;

  }
  //fseek(fileptr,0,SEEK_CUR);
  //saved=ftell(fileptr);
  //printf("The file pointer after the reading is: %ld \n",ftell(fileptr));

  //find the unique pages
  distinct_page(page_number,numOfLines,special_page,page_number);
  //get the training data
  i=0;
  int match=0;
  int no_match;
  for (int z=0;z<train_size;z++)
  {
    x=rand()%total_size;
    train[z]=overall[x];
    train_save[z]=train[z];
    record[z]=x;
    //printf("The training data set is: %d\n",train_save[z].classification);
    //printf("The record number is: %d\n",record[z]);
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
        int y=i-match;
        //printf("The test iteration is:%d\n",y);
        i++;
      }
    }
  //printf("The iteration is:%d\n",i);
  }
  //fclose(fileptr);
  return train_save;
}

void readTrainingData_1(char* filename, int numOfLines, ClassifiedPoint* ret, ClassifiedPoint* test,int size){
  FILE* fileptr;
  
  fileptr = fopen(filename, "r");
  
  int bufferLen = 255;
  char buffer[bufferLen];
  
  int i = 0;
  while(fgets(buffer, bufferLen, fileptr) != NULL&&  i < numOfLines){
    ClassifiedPoint* p =  convertToIrisPoint(buffer);
    if(i<size){
      ret[i] = *p; 
    } 
    //printf("The classification is:%d\n",ret[i].classification);
    //printf("The iteration is: %d\n",i);
    if(i>=size)
    {
      test[i-size]=*p;
    }
    i++;
  }
  //printf("The number of lines are: %d\n",i);
  fclose(fileptr);
 
}

u_int64_t* compare (const void * a, const void * b)
{
  return ( *(u_int64_t*)a - *(u_int64_t*)b );
}

u_int64_t findMajorityClass(ClassifiedDistance*neighbors,int k){
  int count0=0,count1=0;
  u_int64_t compare[k];
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



  //printf("The majority page is:%d\n",compare[res]);
  //int count[2]={0,0};
  //for(int i=0;i<k;++i){
    //if(neighbors[i].classification==1025){
    //count[0]++;
    //}
    //if(neighbors[i].classification==1026){
      //count[1]++;
    //}
    ////if(neighbors[i].classification==1027){
      ////count[2]++;
    ////}
  //}
  //if(count[0]>count[1])
    //{return 1025;}
  //else{
    //return 1026;
  //}
  ////qsort(count,2,sizeof(int),compare);
    ////printf("The final pairing outcome is:%d \n",count[1]);
  ////return count[1];
}

int classify_1(int dimensions, const ClassifiedPoint* trainedData, int trainedSize, u_int64_t* toClassify,int k ){

  //printf("The testing point is: %f %f\n",toClassify[0],toClassify[1]);
  ClassifiedDistance * distances = malloc(sizeof(ClassifiedDistance) * trainedSize);

  //Calculating the Euklid distance to every trained Datapoint
  for (int i = 0; i < trainedSize; i++)
  {
    ClassifiedDistance distance;
    distance.classification = (trainedData)[i].classification;
    distance.distance = eukl_distance(dimensions, (trainedData)[i].point, toClassify);
    distances[i] = distance;
  }

  //Sort the Calculated Distances
  sortCD(distances, trainedSize);
  int final_class;
  final_class=findMajorityClass(distances,k);
  //printf("The final classification class is:%d\n",final_class);

  return final_class;

}

int main_reference (int argc, char const *argv[])
{
  //std::vector<ClassifiedPoint> test_points {};


  //Size of the Dataset to Train
  
  //int trainDataSize = 15;
  //int testDataSize = 985;
  //int totalDataSize =1000;
  
  //Definition of the Meaning of Classes

  //Point that should be Classified
  //float toClassify[2] = {3995210,4207603}; //correct result 1027
  //The K of KNN
  
  //int K = 3;
  
  //Allocate Array to Store trained Data
  ClassifiedPoint *  trainData = malloc(sizeof(ClassifiedPoint) * trainDataSize);
  //Allocate the array to store the test Data
  ClassifiedPoint * testData= malloc (sizeof(ClassifiedPoint)* testDataSize);
  ClassifiedPoint * finalData= malloc(sizeof(ClassifiedPoint)* testDataSize);
  ClassifiedPoint * previous_train= malloc(sizeof(ClassifiedPoint)* trainDataSize);
  ClassifiedPoint * current_train= malloc(sizeof(ClassifiedPoint)* trainDataSize);

  //Read the Trained Data from CSV- File and Store it into TrainData Array
  previous_train= readTrainingData(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
  //readTrainingData_1("605_1644_KNN_modified.csv", totalDataSize, trainData,testData,trainDataSize);
  
  //printf("It can read \n");
  //Classify the Point you want to know of
  for(size_t j=0;j<testDataSize;j++)
  {
    ClassifiedDistance * distances = classify(2, trainData, trainDataSize, testData[j].point);

    //Print the smallest K Results and Display them.
    for (size_t i = 0; i <K; i++)
    {
      
      //printf("Result: The distance is %f, the classification is  %d\n",distances[i].distance, distances[i].classification);
    }

  }
  //ClassifiedDistance * distances = classify(2, trainData, trainDataSize, testData[41].point);
  //printf("Gettting out from the function\n");

  ////Print the smallest K Results and Display them.
  //for (size_t i = 0; i <K; i++)
  //{
    //printf("Result: %f %s\n",distances[i].distance, classes[distances[i].classification]);
    //printf("Result: The distance is %f, the classification is  %d\n",distances[i].distance, distances[i].classification);
  //}

  //int*final_output =findMajorityClass(distances ,3);
  printf("Success\n");
  clock_t begin=clock();
  
  for(size_t j=0;j<testDataSize;j++)
  {
    finalData[j].classification= classify_1(2, trainData, trainDataSize, testData[j].point,K);
  }
  clock_t end= clock();
  double time_taken= (double)(end - begin) / CLOCKS_PER_SEC;
  printf("Time_Taken: %f %s\n",time_taken);

  //accuracy check
  float count_success=0; 
  for(size_t i=0;i<testDataSize;i++)
  {
    if(finalData[i].classification==testData[i].classification)
    {
      //printf("The classification is successful\n");
      count_success++;
    }
  }
  float final_accuracy=count_success/testDataSize;
  printf("The count_success is:%f\n",count_success);
  printf("The testDataSize is:%d\n",testDataSize);
  printf("The final accuracy is:%f \n",final_accuracy);

  //readTrainingData("605_1644_KNN_modified.csv", totalDataSize, trainData,testData,trainDataSize,totalDataSize);  
  int x=0;
  while(x<100)
  {
    if(x==99)
    {
      printf("The number of no replacement is:%d\n",no_replace);
    }
    if(final_accuracy>0.7)
    {
      printf("///////There is no replacement at count:%d//////////\n",x);
      no_replace++;
      //printf("There is no replacement at count:%d\n",x);
      printf("The position is:%d\n",x);
      current_train=readTrainingData(totalDataSize, trainData,testData,trainDataSize,totalDataSize);

      clock_t begin=clock();
  
      for(size_t j=0;j<testDataSize;j++)
      {
        finalData[j].classification= classify_1(2, previous_train, trainDataSize, testData[j].point,K);
      }
      clock_t end= clock();
      double time_taken= (double)(end - begin) / CLOCKS_PER_SEC;
      printf("Time_Taken: %f %s\n",time_taken);

      //accuracy check
      float count_success=0; 
      for(size_t i=0;i<testDataSize;i++)
      {
        if(finalData[i].classification==testData[i].classification)
        {
          //printf("The classification is successful\n");
          count_success++;
        }
      }
      final_accuracy=count_success/testDataSize;
      //printf("In count %d,The count_success is:%f\n",x,count_success);
      //printf("The testDataSize is:%d\n",testDataSize);
      printf("In count %d, The final accuracy is:%f \n",x,final_accuracy);
      x++;
    }
    else if(final_accuracy<=0.7)
    {
      printf("///////There is the replacement at count:%d////////////\n",x);
      current_train=readTrainingData(totalDataSize, trainData,testData,trainDataSize,totalDataSize);
      clock_t begin=clock();
  
      for(size_t j=0;j<testDataSize;j++)
      {
        finalData[j].classification= classify_1(2, current_train, trainDataSize, testData[j].point,K);
      }
      previous_train=current_train;
      clock_t end= clock();
      double time_taken= (double)(end - begin) / CLOCKS_PER_SEC;
      printf("Time_Taken: %f %s\n",time_taken);

      //accuracy check
      float count_success=0; 
      for(size_t i=0;i<testDataSize;i++)
      {
        if(finalData[i].classification==testData[i].classification)
        {
          //printf("The classification is successful\n");
          count_success++;
        }
      }
      final_accuracy=count_success/testDataSize;
      //printf("In count %d,The count_success is:%f\n",x,count_success);
      //printf("The testDataSize is:%d\n",testDataSize);
      printf("In count %d, The final accuracy is:%f \n",x,final_accuracy);
      x++;
    }

  }

  return errno;
}

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  if(champsim::operable::knn_can_predict == true)
  {
  
  ClassifiedPoint *  trainData = malloc(sizeof(ClassifiedPoint) * trainDataSize);
  //Allocate the array to store the test Data
  ClassifiedPoint * testData= malloc (sizeof(ClassifiedPoint)* testDataSize);
  ClassifiedPoint * finalData= malloc(sizeof(ClassifiedPoint)* testDataSize);
  //ClassifiedPoint * previous_train= malloc(sizeof(ClassifiedPoint)* trainDataSize);
  ClassifiedPoint * current_train= malloc(sizeof(ClassifiedPoint)* trainDataSize);

  //Read the Trained Data from CSV- File and Store it into TrainData Array
  current_train= readTrainingData(totalDataSize, trainData,testData,trainDataSize,totalDataSize);

  //Finding the after page number
  for(size_t j=0;j<testDataSize;j++)
  {
    finalData[j].classification= classify_1(2, current_train, trainDataSize, testData[j].point,K);
    for(int x=1;x<=(1<<(LOG2_PAGE_SIZE-LOG2_BLOCK_SIZE));x++)
    {
      uint64_t pf_addr = ((finalData[j].classification)<<LOG2_PAGE_SIZE)+x*64;
      prefetch_line(pf_addr, true, metadata_in);
    }
  }
  //accuracy check
  float count_success=0; 
  for(size_t i=0;i<testDataSize;i++)
  {
    if(finalData[i].classification==testData[i].classification)
    {
      //printf("The classification is successful\n");
      count_success++;
    }
  }
  final_accuracy=count_success/testDataSize;
  printf("The count_success is:%f\n",count_success);
  printf("The testDataSize is:%d\n",testDataSize);
  printf("The final accuracy is:%f \n",final_accuracy);
  
  //previous_train=current_train;
  }

  //accuracy counting

  //uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
  //prefetch_line(pf_addr, true, metadata_in);
  //return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}