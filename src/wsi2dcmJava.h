/*
 * Copyright 2019 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef WSI2DCMJAVA_H
#define WSI2DCMJAVA_H

extern "C" {
int wsi2dcm(char* inputFile, char* outputFileMask, long frameSizeX,
             long frameSizeY, char* compression, int quality, int startOnLevel,
             int stopOnLevel, char* imageName, char* studyId, char* seriesId,
             int retileLevels, double* downsamples, bool tiled, int batchLimit,
             int threads, bool debug);
}

#endif  // WSI2DCMJAVA_H
