#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cmath>

double haversineDistance(double lat1, double lon1, double lat2, double lon2);

const double searchDistance = 50; // kms

using json = nlohmann::json;
using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
   size_t totalSize = size * nmemb;
   std::string* response = static_cast<std::string*>(userp);
   response->append(static_cast<char*>(contents), totalSize);
   return totalSize;
}

int main() {
   // Initialize libcurl
   CURL* curl = curl_easy_init();


   if (curl) {
       cout << "Downloading Dataset" <<  endl;
       std::string url = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson"; // Replace with your JSON URL

       // Perform the HTTP request and store the response
       std::string response;
       curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
       curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
       curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

       CURLcode res = curl_easy_perform(curl);
       curl_easy_cleanup(curl);

       if (res == CURLE_OK) {
           try {

                cout << "Finding Swarms" << endl;
                // Parse the JSON data using nlohmann/json
                json parsed = json::parse(response);
                json earthquakes;

                cout << "Total Earthquakes: " << parsed["features"].size() << endl;
                // filter out earthquakes not within searchDistance of another earthquake;
                for (int i = 0; i < parsed["features"].size(); i++) {
                    //debug
                    //cout << "Searching earthquake " << i << "\n";
                    for (int j = 0; j < parsed["features"].size(); j++) {
                        if (j == i) {
                            j++;
                        }
                        //debug
                        //cout << "    against earthquake " << j << "\n";

                        double a = parsed["features"][i]["geometry"]["coordinates"][0].get<double>();
                        double b = parsed["features"][i]["geometry"]["coordinates"][1].get<double>();
                        double x = parsed["features"][j]["geometry"]["coordinates"][0].get<double>();
                        double y = parsed["features"][j]["geometry"]["coordinates"][1].get<double>();

                        // check distance
                        if  (haversineDistance(a, b, x, y) < searchDistance) {
                            //debug
                            //cout << "       earthquake " << i << " is within searchDistance of earthquake " << j << "\n";
                            earthquakes[0].push_back(parsed["features"][i]);
                            break;
                       }
                    }
                }
                cout << "Relevant Earthquakes: " << earthquakes[0].size() << endl;


                //debug
                //cout << earthquakes[0].dump(4) << endl;

                json swarm;
                int n = 0;
                while (true) {
                    cout << "Search Interation " << n << endl;
                    // if earthquakes[n] is empty, exit loop and print statistics
                    if (earthquakes[n].empty()) {
                        cout << "   Exiting on iteration " << n << endl;
                        break;
                }

                double avgLocation[2];
                avgLocation[0] = earthquakes[n][1]["geometry"]["coordinates"][0].get<double>();
                avgLocation[1] = earthquakes[n][1]["geometry"]["coordinates"][1].get<double>();

                while (true) {
                    // check distance to each earthquake in earthquakes[n]
                    for (int i = 0; i < earthquakes[n].size(); i++) {
                        double x = earthquakes[n][i]["geometry"]["coordinates"][0].get<double>();
                        double y = earthquakes[n][i]["geometry"]["coordinates"][1].get<double>();

                        if (haversineDistance(avgLocation[0], avgLocation[1], x, y) > searchDistance){
                            swarm[n].push_back(earthquakes[n][i]);

                        } else {
                            earthquakes[n+1].push_back(earthquakes[n][i]);
                        }
                    }

                    if (swarm[n].empty()) {
                        earthquakes[n][1].clear();

                        swarm[n].clear();
                        earthquakes[n+1].clear();
                        break;
                    }

                    // Calculate average location of swarm[n]
                    double newAvgLocation[2] = {0,0};
                    for (int i = 0; i < swarm[n].size(); i++) {
                        newAvgLocation[0] += swarm[n][i]["geometry"]["coordinates"][0].get<double>();
                        newAvgLocation[1] += swarm[n][i]["geometry"]["coordinates"][1].get<double>();
                    }

                    newAvgLocation[0] = newAvgLocation[0] / swarm[n].size();
                    newAvgLocation[1] = newAvgLocation[1] / swarm[n].size();

                    if (newAvgLocation[0] == avgLocation[0] && newAvgLocation[1] == avgLocation[1]){
                        break;
                    }
                    avgLocation[0] = newAvgLocation[0];
                    avgLocation[1] = newAvgLocation[1];

                    swarm[n].clear();
                    earthquakes[n+1].clear();
                }
                n++;
            }

            cout << "Found " << swarm.size() << " swarms, calculating statistics" << endl;

            // calculate statistics
            for (int n = 0; n < swarm.size(); n++) {
                double coords[3];
                double mag;

                cout << "Swarm " << n << " contains " << swarm[n].size() << " earthquakes"<< endl;
                //summation
                for (int i = 0; i < swarm[n].size(); i++) {
                    coords[0] += swarm[n][i]["geometry"]["coordinates"][0].get<double>();
                    coords[1] += swarm[n][i]["geometry"]["coordinates"][1].get<double>();
                    coords[2] += swarm[n][i]["geometry"]["coordinates"][2].get<double>();
                    mag +=  swarm[n][i]["properties"]["mag"].get<double>();
                }
                // convert summation into average
                coords[0] /= swarm[n].size();
                coords[1] /= swarm[n].size();
                coords[2] /= swarm[n].size();
                mag /= swarm[n].size();

                // calculate standard deviation


                // print
                cout << "Swarm " << n << ": \n"
                    << "    Location: \n"
                    << "        Center: ("<< coords[0] << ", " << coords[1] << ")\n"
                    // << "        Avg distance from center: " << avgDistance << "\n"
                    << "    Magnitude: \n"
                    << "        Avg: " << mag << "\n"
                    << "    Depth: \n"
                    << "        Avg: " << coords[2] << "km" << endl;
            }

           } catch (const std::exception& e) {
               std::cerr << "JSON parsing error: " << e.what() << std::endl;
           }
       } else {
           std::cerr << "Failed to fetch data from the URL" << std::endl;
       }
   }

   return 0;
}

double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1)*(M_PI/180);
    double dLon = (lon2 - lon1)*(M_PI/180);

    lat1 = lat1*(M_PI/180);
    lat2 = lat2*(M_PI/180);

    // Apply formula
    double a = pow(sin(dLat / 2), 2) +
               pow(sin(dLon / 2), 2) *
               cos(lat1) * cos(lat2);
    double c = 2 * asin(sqrt(a));
    double R = 6371; // Radius of Earth in Kilometers
    return R * c;
}