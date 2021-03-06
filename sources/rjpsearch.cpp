#include "rjpsearch.h"
#include "environmentoptions.h"
#include "ilogger.h"
#include "map.h"
#include "node.h"
#include "opencontainer.h"
#include "list.h"
#include <unordered_set>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "isearch.h"
#include "jpsearch.h"
#include "searchresult.h"


const double PI = 3.1415926;


RJPSearch::RJPSearch()
{

}

RJPSearch::RJPSearch(double weight, int BT, int SL, int distance_to_successors,
             int number_of_successors, int local_search_step_limit)
{
    hweight = weight;
    breakingties = BT;
    sizelimit = SL;

    this->distance_to_successors = distance_to_successors;
    this->number_of_successors = number_of_successors;
    this->local_search_step_limit = local_search_step_limit;

    std::srand(time(NULL));
}

RJPSearch::~RJPSearch()
{

}

SearchResult RJPSearch::startSearch(ILogger *logger, const Map &map, const EnvironmentOptions &options) {
    auto start_time = std::chrono::system_clock::now();

    generateCirleOfSuccessors();

    std::unordered_set<Node> closed;

    OpenContainer<Node> open("g-max");

    Node start;
    start.i = map.start_i;
    start.j = map.start_j;
    start.g = 0;

    Node goal;
    goal.i = map.goal_i;
    goal.j = map.goal_j;

    open.push(start);

    Node current_node, child_node;

    SearchResult localSearchResult;
    sresult.nodescreated = 0;
    sresult.numberofsteps = 0;

    while(!open.empty()) {
        current_node = open.pop();

        if (current_node.AVOID && start != current_node) {
            localSearchResult = findLocalPath(current_node, *current_node.parent, map, logger, options, -1);
            // no step limit if already marked AVOID
        }
        else if (start != current_node) {
            localSearchResult = findLocalPath(current_node, *current_node.parent, map,
                                           logger, options, local_search_step_limit);
        }


        if(localSearchResult.pathfound || start == current_node) {
            if (current_node != start) {
                // Update g from current node with found path
                current_node.g = current_node.parent->g + localSearchResult.pathlength;
                sresult.numberofsteps += localSearchResult.numberofsteps;
                sresult.nodescreated = std::max(sresult.nodescreated, localSearchResult.nodescreated);
            }

            if (current_node == goal) {
                sresult.pathfound = true;
                break;
            }

            auto current_node_iterator = closed.insert(current_node).first;

            for (auto child_coords : generateSuccessors(current_node, map, closed)) {
                child_node = Node(child_coords.first, child_coords.second);

                child_node.parent = &(*current_node_iterator); // FIXME: not sure all this is a must

                // First we put this in g - path length to parent
                // plus distance_to_successors value, as we won't get
                // there faster
                child_node.g = current_node.g + distance_to_successors;

                calculateHeuristic(child_node, map, options);

                if (!isNodePerpective(child_node, map, options)) child_node.AVOID = true;

                open.push(child_node);
            }
        }
        else if(!current_node.AVOID) {
            current_node.AVOID = true;
                open.push(current_node);
        }
        sresult.numberofsteps += 1;
    }

    auto end_time = std::chrono::system_clock::now();
    sresult.time = (std::chrono::duration<double>(end_time - start_time)).count();

    if (sresult.pathfound) {
        sresult.hppath = new NodeList();
        sresult.lppath = new NodeList();

        SearchResult localSearchResult;
        while (current_node.parent != nullptr) {
            localSearchResult = findLocalPath(current_node, *current_node.parent,
                                              map, logger, options, -1);
            sresult.JoinLpPaths( localSearchResult );
            sresult.hppath->push_front(current_node);
            current_node = *current_node.parent;

            sresult.numberofsteps += localSearchResult.numberofsteps;
            sresult.nodescreated = std::max(sresult.nodescreated, localSearchResult.nodescreated);
        }

        sresult.nodescreated += open.size() + closed.size();
        sresult.hppath->push_front(current_node); // add start node
    }

    return sresult;
}

SearchResult RJPSearch::findLocalPath(const Node &node, const Node &parent_node, const Map &map,
                          ILogger *logger, const EnvironmentOptions &options, int localSearchStepLimit)
{
    SearchResult localSearchResult;
    if (!node.AVOID) {
        if (lineOfSight(node, parent_node, map)) {
            localSearchResult.pathfound = true;
            localSearchResult.nodescreated = 0;
            localSearchResult.numberofsteps = 1;
            localSearchResult.pathlength = sqrt((node.i - parent_node.i) * (node.i - parent_node.i) +
                                                (node.j - parent_node.j) * (node.j - parent_node.j));

            localSearchResult.hppath = new NodeList;
            localSearchResult.lppath = new NodeList;
            localSearchResult.hppath->push_front(node);
            localSearchResult.hppath->push_front(parent_node);
            localSearchResult.lppath->push_front(node);
            localSearchResult.lppath->push_front(parent_node);
        }
        else {
            localSearchResult.pathfound = false;
            localSearchResult.nodescreated = 0;
            localSearchResult.numberofsteps = 1;
        }
        return localSearchResult;
    }
    JPSearch localSearch(hweight, breakingties, 0); // no limit
    localSearch.setAlternativePoints(parent_node, node);
    localSearchResult = localSearch.startSearch(logger, map, options);

    return localSearchResult;
}

void RJPSearch::calculateHeuristic(Node & a, const Map &map, const EnvironmentOptions &options)
{
    a.F = a.g;
    int di = abs(a.i - map.goal_i),
        dj = abs(a.j - map.goal_j);

    // Normalizing heuristics with linecost
    if(options.metrictype == CN_SP_MT_EUCL) a.H = sqrt(di * di + dj * dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_MANH) a.H = (di + dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_DIAG) a.H = std::min(di, dj) * options.diagonalcost +
                                                                        abs(di - dj) * options.linecost;
    else a.H = std::max(di, dj) * options.linecost;

    a.F += hweight * a.H;
}

bool RJPSearch::isNodePerpective(Node &a, const Map &map, const EnvironmentOptions &options) {
    double H, dH; // H - start to a, dH - a to a.parent

    int di = abs(a.i - map.start_i),
        dj = abs(a.j - map.start_j);

    if(options.metrictype == CN_SP_MT_EUCL)       H = sqrt(di * di + dj * dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_MANH) H = (di + dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_DIAG) H = std::min(di, dj) * options.diagonalcost +
                                                                        abs(di - dj) * options.linecost;
    else H = std::max(di, dj) * options.linecost;

    /*di = abs(a.i - a.parent->i);
    dj = abs(a.j - a.parent->j);

    if(options.metrictype == CN_SP_MT_EUCL)       dH = sqrt(di * di + dj * dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_MANH) dH = (di + dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_DIAG) dH = std::min(di, dj) * options.diagonalcost +
                                                                        abs(di - dj) * options.linecost;
    else dH = std::max(di, dj) * options.linecost;*/

    return (a.parent->g + distance_to_successors < hweight * H);
}

std::vector<std::pair<int, int> > RJPSearch::generateSuccessors(const Node &node, const Map &map,
                                                            const std::unordered_set<Node> &closed)
{
    std::vector< std::pair<int, int> > successors;

    Node parent;
    bool has_parent = false;
    if (node.parent != nullptr) {
        parent = *node.parent;
        has_parent = true;
    }

    std::pair<int, int> successor;

    auto it = successors_circle.begin();
    for(; it < successors_circle.end(); it++) {
        successor = *it;

        successor.first = node.i + successor.first;
        successor.second = node.j + successor.second;

        if(!map.CellOnGrid(successor.first, successor.second)) {
            continue;
        }
        if(map.CellIsObstacle(successor.first, successor.second)) {
            continue;
        }
        if (has_parent) {
            if (parent.i == successor.first && parent.j == successor.second) {
                continue;
            }
        }

        successors.push_back(successor);
    }

    int element_to_erase;
    while(successors.size() > number_of_successors) {
        element_to_erase = rand() % successors.size();

        successors.erase(successors.begin() + element_to_erase);
    }

    if ((node.i - map.goal_i) * (node.i - map.goal_i) +
            (node.j - map.goal_j) * (node.j - map.goal_j) <=
                (int) (distance_to_successors * distance_to_successors)) {
        successors.push_back(std::pair<int, int> (map.goal_i, map.goal_j));
    }

    return successors;
}


void RJPSearch::generateCirleOfSuccessors()
{
    successors_circle.resize(0);
    int di = distance_to_successors,
        dj = 0;

    for(; di >= 0 && di >= dj; di--) {
        while (dj * dj + di * di <= distance_to_successors * distance_to_successors) {
            successors_circle.push_back(std::pair<int,int>(di, dj));
            successors_circle.push_back(std::pair<int,int>(dj, di));
            successors_circle.push_back(std::pair<int,int>(-di, dj));
            successors_circle.push_back(std::pair<int,int>(-dj, di));
            successors_circle.push_back(std::pair<int,int>(di, -dj));
            successors_circle.push_back(std::pair<int,int>(dj, -di));
            successors_circle.push_back(std::pair<int,int>(-di, -dj));
            successors_circle.push_back(std::pair<int,int>(-dj, -di));
            dj++;
        }
    }

    std::sort(successors_circle.begin(), successors_circle.end());
    successors_circle.erase(std::unique(successors_circle.begin(), successors_circle.end()),
                            successors_circle.end());
}


bool RJPSearch::lineOfSight(const Node &p, const Node &q, const Map &map)
{
    int x_1 = p.i,
        y_1 = p.j,
        x_2 = q.i,
        y_2 = q.j;

    if (x_2 < x_1) {
        // to ensure x grows from x_1 to x_2
        int temp = x_1;
        x_1 = x_2;
        x_2 = temp;

        temp = y_1;
        y_1 = y_2;
        y_2 = temp;
    }

    int dx = x_2 - x_1,
        dy = y_2 - y_1;

    // a * x + b * y + c = 0, equotation of the line through (x1, y1) and (x2, y2)
    // multiplying by 2 to check grid intersection points easily (add 1 instead of 0.5 to each of coordinates, ints only)
    int a = 2 * dy,
        b = - 2 * dx,
        c = 0 - a * x_1 - b * y_1;

    int growth_y;
    if (dy > 0) growth_y = 1;
    else        growth_y = -1;

    int current_x = x_1,
        current_y = y_1;

    while (current_x != x_2 || current_y != y_2) {
        if (map.CellIsObstacle(current_x, current_y)) return false;

        // let's check if the line (a, b, c) will cross the Ox-parallel line of grid in this cell
        if (a * current_x + a / 2 + b * current_y + b * growth_y / 2 + c == 0) {
            // this happens when we're on the intersection, just skip forward, we can squeeze around corner
            current_x += 1;
            current_y += growth_y;
        }
        else if (growth_y * (a * current_x + a / 2 + b * current_y + b * growth_y / 2 + c) < 0)
                        current_x += 1; // not crossing at current x, move horizontally
        else            current_y += growth_y; // crossing, move vertically
    }

    return true;
}
