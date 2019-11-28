# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

apt-get install wget -y
accessToken=$ACCESS_TOKEN
repositoryString=$1
tagName=$2
version=${tagName:1}
IFS='_' read -r -a repoArray <<< "$repositoryString"
githubUser="${repoArray[1]}"
githubRepo="${repoArray[2]}"
if [ -z "$githubUser" ]
then
    githubUser="GoogleCloudPlatform"
    githubRepo=$1
fi
zipName="$tagName.zip"
zipUrl="https://github.com/$githubUser/$githubRepo/archive/$zipName"

#method to upload files to github release page
upload () {
  path=$1
  file=$2
  responseCode=$(curl -# -XPOST -H "Authorization:token "$accessToken -H "Content-Type:application/octet-stream" \
--data-binary @"$path/$file" -w "%{http_code}" https://uploads.github.com/repos/$githubUser/$githubRepo/releases/$releaseId/assets?name=$file -o response.json)
  if [ $responseCode -ne 201 ]; then
    cat response.json
    exit 1;
  fi
}

echo "release: $tagName"
echo "user: $githubUser"
echo "repo: $githubRepo"
echo "zipUrl: $zipUrl"

# request to create release
echo {\"tag_name\": \"$tagName\",\"name\": \"$tagName\",\"body\": \"$tagName\"} > /workspace/request.json
responseCode=$(curl -# -XPOST -H 'Content-Type:application/json' -H 'Accept:application/json' -w "%{http_code}" --data-binary @/workspace/request.json \
https://api.github.com/repos/$githubUser/$githubRepo/releases?access_token=$accessToken -o response.json)  
if [ $responseCode -ne 201   ]; then
    cat response.json
    exit 1;
fi

wget $zipUrl
zipSha=$(sha256sum $zipName)
homebrewFile="wsi2dcm.rb"
homebrewPath="/workspace/cloud_build"
homebrewFullPath="$homebrewPath/$homebrewFile"
zipUrl=$(echo "$zipUrl" | sed 's/\//\\\//g')
sed -i "s/version \"\"/version \"$version\"/g" $homebrewFullPath
sed -i "s/url \"\"/url \"$zipUrl\"/g" $homebrewFullPath
sed -i "s/sha256 \"\"/sha256 \"${zipSha:0:64}\"/g" $homebrewFullPath

#get id of new release
releaseId=$(grep -wm 1 "id" /workspace/response.json | grep -Eo "[[:digit:]]+")

#upload bins to release page
upload /workspace/build wsi2dcm 
upload /workspace/build libwsi2dcm.so 
upload /workspace/cloud_build/deb wsi2dcm_"$version".deb
upload /workspace/cloud_build/rpm/RPMS/x86_64 wsi2dcm-"$version"-1.x86_64.rpm 
upload $homebrewPath $homebrewFile
