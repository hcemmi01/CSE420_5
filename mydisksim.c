#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SECTORSPERTRACK 200
#define TRACKSPERCYLINDER 8
#define CYLINDERS 500000
#define RPM 10000
#define PHYSICALSECTORSIZE 512
#define LOGICALBLOCKSIZE 4096
const double TRANSFERRATE = 1000000000;

typedef struct {
  double arrival_time;
  int lbn;
  int request_size;
  //stuff from part 2...
  int psn;
  int cylinder;
  int surface;
  double sector_offset;
  } DiskRequest;

void calculate_disk_geometry(DiskRequest *request) {
  request->psn = request->lbn*8;
  request->cylinder = request->psn/(SECTORSPERTRACK * TRACKSPERCYLINDER);
  int sectorsPerCylinder = SECTORSPERTRACK * TRACKSPERCYLINDER;
  request->surface = (request->psn % sectorsPerCylinder) / SECTORSPERTRACK;
  request->sector_offset = request->psn % SECTORSPERTRACK;
}

//head starts at cylinder 0

void fcfs(DiskRequest *requests, int count, const char *outputFile) {
    FILE *file = fopen(outputFile, "w");
    if (file == 0) {
        printf("Error opening the output file during FCFS\n");
        return;
    }
    // Requests are already sorted by arrival time
    double currentTime = 0;
    int currCylinder = 0;
    double currentSectorOffset = 0;

    for (int i = 0; i < count; i++) {
        if (currentTime < requests[i].arrival_time) {
            currentTime = requests[i].arrival_time;
        }
        int seekDistance = abs(currCylinder - requests[i].cylinder);
        double seekTime;
        if (seekDistance == 0) {
            seekTime = 0;
        } else {
            seekTime = ((0.000028 * seekDistance) + 2) / 1000.0;
        }
        double rotationPeriod = 60.0 / RPM;
        double sectorsPerSecond = SECTORSPERTRACK / rotationPeriod;
        double sectorsPassedDuringSeek = seekTime * sectorsPerSecond;
        currentSectorOffset += sectorsPassedDuringSeek;
        currentSectorOffset = fmod(currentSectorOffset, SECTORSPERTRACK);
        double travelTimePerSector = rotationPeriod / SECTORSPERTRACK;
        double sectorsToGo;
        if (currentSectorOffset <= requests[i].sector_offset) {
            sectorsToGo = requests[i].sector_offset - currentSectorOffset;
        } else {
            sectorsToGo = SECTORSPERTRACK - (currentSectorOffset - requests[i].sector_offset);
        }
        double rotationalLatency = sectorsToGo * travelTimePerSector;
        double transferTime = (requests[i].request_size * LOGICALBLOCKSIZE) / TRANSFERRATE;
        double waitTime;
        if (currentTime > requests[i].arrival_time) {
            waitTime = currentTime - requests[i].arrival_time;
        } else {
            waitTime = 0;
        }
        double serviceTime = seekTime + rotationalLatency + transferTime;
        double finishTime = requests[i].arrival_time + waitTime + serviceTime;
        currentTime = finishTime;
        currCylinder = requests[i].cylinder;
        int psn_after = requests[i].psn + requests[i].request_size;
        double sectorsTransferred = requests[i].request_size; // Since in sample output they add request_size
        currentSectorOffset = requests[i].sector_offset + sectorsTransferred;
        currentSectorOffset = fmod(currentSectorOffset, SECTORSPERTRACK);
        int sectorsPerCylinder = SECTORSPERTRACK * TRACKSPERCYLINDER;
        int surface = (psn_after % sectorsPerCylinder) / SECTORSPERTRACK;

        // Debug Logs
        /*printf("  Arrival Time: %lf\n", requests[i].arrival_time);
        printf("  Seek Distance: %d, Seek Time: %lf\n", seekDistance, seekTime);
        printf("  Rotational Latency: %lf, Sectors To Go: %lf\n", rotationalLatency, sectorsToGo);
        printf("  Transfer Time: %lf\n", transferTime);
        printf("  Wait Time: %lf\n", waitTime);
        printf("  Finish Time: %lf\n", finishTime);*/

        // Output to file: arrival time, finish time, waiting time, psn_after, cylinder, surface, sector offset, seek distance
        fprintf(file, "%lf %lf %lf %d %d %d %lf %d\n",
                requests[i].arrival_time, finishTime, waitTime,
                psn_after, currCylinder, surface, currentSectorOffset, seekDistance);
    }
    fclose(file);
}

void sstf(DiskRequest *requests, int count, const char *outputFile) {
    FILE *file = fopen(outputFile, "w");
    if (file == 0) {
        printf("Error opening the output file during SSTF\n");
        return;
    }

    double currentTime = 0;
    int currCylinder = 0;
    double currentSectorOffset = 0;
    int *processed = (int *)calloc(count, sizeof(int));
    int processedCount = 0;

    while (processedCount < count) {
        int nextRequestIndex = -1;
        int shortestSeekDistance = 2147483647;

        //shortest time seach
        for (int i = 0; i < count; i++) {
            if (!processed[i] && requests[i].arrival_time <= currentTime) {
                int seekDistance = abs(currCylinder - requests[i].cylinder);
                if (seekDistance < shortestSeekDistance) {
                    shortestSeekDistance = seekDistance;
                    nextRequestIndex = i;
                }
            }
        }

        //move time forward
        if (nextRequestIndex == -1) {
            for (int i = 0; i < count; i++) {
                if (!processed[i]) {
                    nextRequestIndex = i;
                    currentTime = requests[i].arrival_time;
                    break;
                }
            }
        }

        if (nextRequestIndex == -1) {
            break;
        }

        DiskRequest *request = &requests[nextRequestIndex];
        processed[nextRequestIndex] = 1;
        processedCount++;
        //calculate info
        int seekDistance = abs(currCylinder - request->cylinder);
        double seekTime = (seekDistance == 0) ? 0 : ((0.000028 * seekDistance) + 2) / 1000.0;
        double rotationPeriod = 60.0 / RPM;
        double sectorsPerSecond = SECTORSPERTRACK / rotationPeriod;
        double sectorsPassedDuringSeek = seekTime * sectorsPerSecond;
        currentSectorOffset += sectorsPassedDuringSeek;
        currentSectorOffset = fmod(currentSectorOffset, SECTORSPERTRACK);
        double travelTimePerSector = rotationPeriod / SECTORSPERTRACK;
        double sectorsToGo = (currentSectorOffset <= request->sector_offset) ?
                             request->sector_offset - currentSectorOffset :
                             SECTORSPERTRACK - (currentSectorOffset - request->sector_offset);
        double rotationalLatency = sectorsToGo * travelTimePerSector;
        double transferTime = (request->request_size * LOGICALBLOCKSIZE) / TRANSFERRATE;
        double waitTime = (currentTime > request->arrival_time) ?
                          currentTime - request->arrival_time : 0;
        double serviceTime = seekTime + rotationalLatency + transferTime;
        double finishTime = request->arrival_time + waitTime + serviceTime;
        currentTime = finishTime;
        currCylinder = request->cylinder;
        int psn_after = request->psn + request->request_size;
        double sectorsTransferred = request->request_size;
        currentSectorOffset = request->sector_offset + sectorsTransferred;
        currentSectorOffset = fmod(currentSectorOffset, SECTORSPERTRACK);
        int sectorsPerCylinder = SECTORSPERTRACK * TRACKSPERCYLINDER;
        int surface = (psn_after % sectorsPerCylinder) / SECTORSPERTRACK;

        fprintf(file, "%lf %lf %lf %d %d %d %lf %d\n",
                request->arrival_time, finishTime, waitTime,
                psn_after, currCylinder, surface, currentSectorOffset, seekDistance);
    }

    fclose(file);
    free(processed);
}

int sort_by_cylinder(const void *a, const void *b) {
    DiskRequest *reqA = (DiskRequest *)a;
    DiskRequest *reqB = (DiskRequest *)b;
    return reqA->cylinder - reqB->cylinder;
}

void scan(DiskRequest *requests, int count, const char *outputFile) {
    FILE *file = fopen(outputFile, "w");
    if (!file) {
        printf("Error opening the output file during SCAN\n");
        return;
    }

    double currentTime = requests[0].arrival_time;
    int currCylinder = 0;
    double currentSectorOffset = 0;
    int *processed = (int *)calloc(count, sizeof(int));
    int direction = 1; // 1 for forward, -1 for backward
    int *ready = (int *)calloc(count, sizeof(int));

    while (1) {
        int readyCount = 0;

        //building ready list
        for (int i = 0; i < count; i++) {
            if (!processed[i] && requests[i].arrival_time <= currentTime) {
                ready[readyCount] = i;
                readyCount++;
            }
        }
        if (readyCount == 0) break;

        //sort ready list by cylinder
        qsort(ready, readyCount, sizeof(int), sort_by_cylinder);

        //finding next request in the current direction
        int nextRequestIndex = -1;
        int shortestDistance = CYLINDERS;

        for (int i = 0; i < readyCount; i++) {
            DiskRequest *req = &requests[ready[i]];
            int distance = abs(req->cylinder - currCylinder);
            if (direction == 1 && req->cylinder >= currCylinder) {
                if (distance < shortestDistance) {
                    shortestDistance = distance;
                    nextRequestIndex = ready[i];
                }
            }
            else if (direction == -1 && req->cylinder <= currCylinder) {
                if (distance < shortestDistance) {
                    shortestDistance = distance;
                    nextRequestIndex = ready[i];
                }
            }
        }

        //reversing direction if no valid request found (THIS IS WRONG!! SHOULD FINISH THROUGH ALL CYLINDERS FIRST)
        if (nextRequestIndex == -1) {
            direction = -direction;
            continue;
        }

        //processing request
        DiskRequest *request = &requests[nextRequestIndex];
        processed[nextRequestIndex] = 1;

        int seekDistance = abs(currCylinder - request->cylinder);
        double seekTime = (seekDistance == 0) ? 0 : ((0.000028 * seekDistance) + 2) / 1000.0;
        double rotationPeriod = 60.0 / RPM;
        double sectorsPerSecond = SECTORSPERTRACK / rotationPeriod;
        double sectorsPassedDuringSeek = seekTime * sectorsPerSecond;
        currentSectorOffset += sectorsPassedDuringSeek;
        currentSectorOffset = fmod(currentSectorOffset, SECTORSPERTRACK);
        double sectorsToGo = (currentSectorOffset <= request->sector_offset) ? request->sector_offset - currentSectorOffset : SECTORSPERTRACK - (currentSectorOffset - request->sector_offset);
        double rotationalLatency = sectorsToGo * (rotationPeriod / SECTORSPERTRACK);
        double transferTime = (request->request_size * LOGICALBLOCKSIZE) / TRANSFERRATE;
        double waitTime = fmax(0, currentTime - request->arrival_time);
        double serviceTime = seekTime + rotationalLatency + transferTime;
        double finishTime = currentTime + serviceTime;
        currentTime = finishTime;
        currCylinder = request->cylinder;
        currentSectorOffset = fmod(request->sector_offset + request->request_size, SECTORSPERTRACK);
        fprintf(file, "%lf %lf %lf %d %d %d %lf %d\n", request->arrival_time, finishTime, waitTime, request->psn + request->request_size, request->cylinder, request->surface, currentSectorOffset, seekDistance);
    }

    fclose(file);
    free(processed);
    free(ready);
}

int compare_by_cylinder(const void *a, const void *b) {
    return ((DiskRequest *)a)->cylinder - ((DiskRequest *)b)->cylinder;
}

void clook(DiskRequest *requests, int count, const char *outputFile) {
    double totalWaitTime = 0, totalSeekTime = 0, totalRotationalLatency = 0;
    double totalTransferTime = 0, totalServiceTime = 0, totalFinishTime = 0;
    //increasing order
    qsort(requests, count, sizeof(DiskRequest), compare_by_cylinder);
    FILE *output = fopen(outputFile, "w");
    if (output == 0) {
        printf("Error opening output file.\n");
        return;
    }
    int currentCylinder = 0; // Initial cylinder position
    for (int i = 0; i < count; i++) {
        DiskRequest *request = &requests[i];
        double waitTime = fmax(0, totalFinishTime - request->arrival_time);
        int seekDistance = abs(currentCylinder - request->cylinder);
        double seekTime = 0;
        if (seekDistance != 0) {
            seekTime = ((0.000028 * seekDistance) + 2) / 1000.0;
        }
        totalSeekTime += seekTime;

        double rotationPeriod = 60.0 / RPM;
        double sectorsPerTrack = SECTORSPERTRACK;

        int currentTrack = currentCylinder / sectorsPerTrack;
        int requestedTrack = request->cylinder / sectorsPerTrack;

        //sector calc
        int sectorsToGo = abs(currentTrack - requestedTrack);

        double travelTimePerSector = rotationPeriod / sectorsPerTrack;
        double rotationalLatency = sectorsToGo * travelTimePerSector;
        totalRotationalLatency += rotationalLatency;
        double transferTime = (request->request_size * LOGICALBLOCKSIZE) / TRANSFERRATE;
        totalTransferTime += transferTime;
        //service time
        double serviceTime = seekTime + rotationalLatency + transferTime;
        totalServiceTime += serviceTime;
        totalFinishTime = request->arrival_time + waitTime + serviceTime;
        //if (totalFinishTime < request->arrival_time) {
            //totalFinishTime = request->arrival_time;
        //}
        currentCylinder = request->cylinder;
        //psn
        request->psn = request->cylinder * SECTORSPERTRACK + request->sector_offset;
        //request->surface = request->cylinder / TRACKSPERCYLINDER;
        //print
        fprintf(output, "%.6f %.6f %.6f %d %d %d %.6f %d\n",
                request->arrival_time, totalFinishTime, waitTime,
                request->psn, request->cylinder, request->surface,
                request->sector_offset, seekDistance);
    }
    // Close the output file
    fclose(output);
}


int main(int argc, char *argv[]) {
    if (argc != 4 && argc != 5) {
      return 1;
    }

    char *inputFile = argv[1];
    char *outputFile = argv[2];
    char *alg = argv[3];
    int limit = -1;
    if (argc == 5) {
      limit = atoi(argv[4]);
    }

    FILE *file = fopen(inputFile, "r");
    if (file == 0) {
      printf("Error opening the input file\n");
      return 1;
    }

    //create a dynamic array of requests of type struct DiskRequest
    int capacity = 50;
    int count = 0;
    DiskRequest *requests = (DiskRequest *)malloc(sizeof(DiskRequest) * capacity);
    if (requests == 0) {
      printf("Error allocating memory for requests\n");
      fclose(file);
      return 1;
    }

    //parse and populate our array with all of the requests from the file
    char input[1024];
    while (fgets(input, 1024, file)) {
      if (count >= capacity) {
        capacity *= 2;
        requests = (DiskRequest *)realloc(requests, capacity * sizeof(DiskRequest));
        if (requests == 0) {
          printf("Error reallocating memory for requests\n");
          fclose(file);
          return 1;
        }
      }
      //char arrival[100], lbn[100], requestSize[100];
      sscanf(input, "%lf %d %d", &requests[count].arrival_time, &requests[count].lbn, &requests[count].request_size);
      calculate_disk_geometry(&requests[count]); //populate the parameters from part 2
      count++;
      if (limit != -1 && count >= limit) {
        break; // make sure that if the optional limit is given to stop at the limit
      }
    }
    fclose(file);

    //debugging, comment this out later...
    //printf("Read %d requests\n", count);
    // for (int i = 0; i < count; i++) {
    //  printf("Request # %d -- Arrival Time: %lf, LBN: %d, Request Size: %d\nPSN: %d, Cylinder Number: %d, Surface Number: %d, Sector offset: %lf\n\n", i+1, requests[i].arrival_time, requests[i].lbn, requests[i].request_size, requests[i].psn, requests[i].cylinder, requests[i].surface, requests[i].sector_offset);
    //} //debugging...

    if (strcmp(alg, "FCFS") == 0 || strcmp(alg, "fcfs") == 0) {
      fcfs(requests, count, outputFile);
    } else if (strcmp(alg, "SSTF") == 0 || strcmp(alg, "sstf") == 0) {
      sstf(requests, count, outputFile);
    } else if (strcmp(alg, "SCAN") == 0 || strcmp(alg, "scan") == 0) {
      scan(requests, count, outputFile);
    } else if (strcmp(alg, "CLOOK") == 0 || strcmp(alg, "clook") == 0) {
      clook(requests, count, outputFile);
    } else {
      printf("Invalid algorithm given. Terminating program...\n");
      free(requests);
      exit(1);
    }

    free(requests);
    //printf("Done!\n");
    return 0;
}