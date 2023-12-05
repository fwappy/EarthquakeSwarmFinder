#include <iostream>
#include <curl/curl.h>
#include "json.hpp"
#include <cmath>

using json = nlohmann::json;
using namespace std;

const double searchDistance = 50; // kms
const int minSwarmSize = 5;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
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

void filterEarthquakes(json& filtered, const json& raw){
    int size = raw["features"].size();

    cout << "Total Earthquakes: " << size << "\n";
    // filter out earthquakes not within searchDistance of another earthquake;
    for (int i = 0; i < size; i++) {
        //debug
        //cout << "Searching earthquake " << i << "\n";
        for (int j = 0; j < size; j++) {
            if (j == i) {
                j++;
            }
            if (j >= size) {
                break;
            }
            //debug
            //cout << "    against earthquake " << j << "\n";

            double a = raw["features"][i]["geometry"]["coordinates"][0].get<double>();
            double b = raw["features"][i]["geometry"]["coordinates"][1].get<double>();
            double x = raw["features"][j]["geometry"]["coordinates"][0].get<double>();
            double y = raw["features"][j]["geometry"]["coordinates"][1].get<double>();

            // check distance
            double distance = haversineDistance(a, b, x, y);
            if  (distance < searchDistance) {
                //debug
                cout << fixed << setprecision(2) << "       earthquake " << i << " is " << distance << "km from earthquake " << j << "\n";
                filtered.push_back(raw["features"][i]);
                break;
            }
        }
    }
    cout << "Relevant Earthquakes: " << filtered.size() << "\n";
}


void findSwarms (json& swarms, json& earthquakes){
    int n = 0;
    while (true) {
        int recalculations = 0;
        // if earthquakes[n] is empty, exit loop and print statistics
        if (earthquakes[n].empty()) {
            break;
        }
        cout << "\nSwarm " << n << ":\n";

        double avgLocation[2];
        avgLocation[0] = earthquakes[n][0]["geometry"]["coordinates"][0].get<double>();
        avgLocation[1] = earthquakes[n][0]["geometry"]["coordinates"][1].get<double>();

        while (true) {
            // check distance to each earthquake in earthquakes[n]
            for (int i = 0; i < earthquakes[n].size(); i++) {
                double x = earthquakes[n][i]["geometry"]["coordinates"][0].get<double>();
                double y = earthquakes[n][i]["geometry"]["coordinates"][1].get<double>();

                double distance = haversineDistance(avgLocation[0], avgLocation[1], x, y);
                //debug
                //cout << "       earthquake " << i << " is " << distance << "km from average" << "\n";

                if (distance < searchDistance){
                    cout << "           Earthquake " << i << " is " << distance << "km from center of swarm " << n << "\n";
                    swarms[n].push_back(earthquakes[n][i]);

                } else {
                    earthquakes[n+1].push_back(earthquakes[n][i]);
                }
            }

            // Calculate average location of swarms[n]
            double newAvgLocation[2] = {0,0};
            for (int i = 0; i < swarms[n].size(); i++) {
                newAvgLocation[0] += swarms[n][i]["geometry"]["coordinates"][0].get<double>();
                newAvgLocation[1] += swarms[n][i]["geometry"]["coordinates"][1].get<double>();
            }

            newAvgLocation[0] = newAvgLocation[0] / swarms[n].size();
            newAvgLocation[1] = newAvgLocation[1] / swarms[n].size();

            if (newAvgLocation[0] == avgLocation[0] && newAvgLocation[1] == avgLocation[1]){
                cout << "   average did not change, exiting" << "\n";


                //swarms[n]["statistics"] = json::array();

                //store newAvgLocation to swarms[n]["statistics"]["coordinates"]
                //swarms[n]["statistics"]["coordinates"][0] = newAvgLocation[0];
                //swarms[n]["statistics"]["coordinates"][1] = newAvgLocation[1];

                break;
            }
            recalculations++;
            cout << "   average changed, recalculating x" << recalculations << "\n";
            avgLocation[0] = newAvgLocation[0];
            avgLocation[1] = newAvgLocation[1];

            //prepare for recalculation
            swarms[n].clear();
            earthquakes[n+1].clear();
        }
        n++;
    }

    cout << "Found " << swarms.size() << " swarms\n\n";
}

void cullSwarms(json& output, const json& input){
    cout << "Culling swarms smaller than size " << minSwarmSize << "\n";
    for (int n = 0; n < input.size(); n++) {
        if (input[n].size() >= minSwarmSize) {
            output.push_back(input[n]);
        } else {
            cout << "   swarm " << n << " is size " << input[n].size() << ", culling\n";
        }
    }
    cout << output.size() << " swarms remaining" << "\n\n";
}

void calculateStatistics(json& swarms) {
    for (int n = 0; n < swarms.size(); n++) {
        int count = swarms[n].size();
        // calculate average
        double depthAvg;
        double magAvg;
        double distAvg;

        int a = swarms[n]["statistics"]["coordinates"][0];
        int b = swarms[n]["statistics"]["coordinates"][1];
        //summation
        for (int i = 0; i < count; i++) {
            depthAvg += swarms[n][i]["geometry"]["coordinates"][2].get<double>();
            magAvg += swarms[n][i]["properties"]["mag"].get<double>();

            int x = swarms[n][i]["geometry"]["coordinates"][0];
            int y = swarms[n][i]["geometry"]["coordinates"][1];
            distAvg += haversineDistance(a, b, x, y);
        }
        // convert summation into average
        depthAvg /= count;
        magAvg /= count;
        distAvg /= count;

        // calculate standard deviation
        double depthSD;
        double magSD;
        double distSD;
        for (int i = 0; i < count; i++) {
            depthSD += (swarms[n][i]["geometry"]["coordinates"][2].get<double>() - depthAvg);
            magSD +=  (swarms[n][i]["properties"]["mag"].get<double>() - magAvg);

            int x = swarms[n][i]["geometry"]["coordinates"][0];
            int y = swarms[n][i]["geometry"]["coordinates"][1];
            distSD += (haversineDistance(a, b, x, y) - distAvg);
        }
        depthSD = sqrt(depthSD / count);
        magSD = sqrt(magSD / count);
        distSD = sqrt(distSD / count);

        //store to swarms[n]["statistics"]
        json statistics;
        statistics["depth"]["avg"] = depthAvg;
        statistics["depth"]["standardDeviation"] = depthSD;
        statistics["mag"]["avg"] = magAvg;
        statistics["mag"]["standardDeviation"] = magSD;
        statistics["dist"]["avg"] = distAvg;
        statistics["dist"]["standardDeviation"] = distSD;

        // check if "statistics" field exists, if not, create it
        if (!swarms[n].contains("statistics")) {
            swarms[n]["statistics"] = json::object();
        }

        // store statistics in swarms[n]["statistics"]
        swarms[n]["statistics"] = statistics;
    }
}

void printStatistics(json& swarms) {
    for (int n = 0; n < swarms.size(); n++) {
        cout << "\nSwarm " << n << ":\n"
        << swarms[n]["statistics"].dump(4) << "\n";
    }
}

int main() {
    // Initialize libcurl
    CURL* curl = curl_easy_init();


    if (curl) {
        cout << "Downloading Dataset\n";
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


                cout << "Finding Swarms\n";
                // Parse the JSON data using nlohmann/json
                json parsed = json::parse(response);
                json earthquakes = json::array({json::array()});

                filterEarthquakes(earthquakes[0], parsed);

                json swarms;
                findSwarms(swarms, earthquakes);

                json output;
                cullSwarms(output, swarms);

                calculateStatistics(output);

                printStatistics(output);

            //     cout << "\nSwarm " << n << ": (" << swarms[n].size() << " earthquakes)\n"
            //     << "    Location: \n"
            //     << "        Center: ("<< coords[1] << ", " << coords[0] << ")\n"
            //     // << "        Avg distance from center: " << avgDistance << "\n"
            //     << "    Magnitude: \n"
            //     << "        Avg: " << mag << "\n"
            //     << "    Depth: \n"
            //     << "        Avg: " << coords[2] << "km" << endl;
            // }

        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << "\n";
        }
    } else {
        std::cerr << "Failed to fetch data from the URL\n";
    }
}

return 0;
}
