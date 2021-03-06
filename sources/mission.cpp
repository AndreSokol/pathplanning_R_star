#include "mission.h"
#include "astar.h"
#include "jpsearch.h"
#include "xmllogger.h"
#include "gl_const.h"
#include "thetastar.h"
#include "rstar.h"
#include "rjpsearch.h"

Mission::Mission()//� config � map - ���� ���� ������������ �� ���������, �� ����� �� ���� ����������������.
{
    logger = NULL;
    search = NULL;
    fileName = NULL;
}

Mission::Mission(const char* FileName)
{
    fileName = FileName;
    logger = NULL;
    search = NULL;
}

Mission::~Mission()
{
    if (logger) delete logger;
    if (search) delete search;
}

bool Mission::getMap()
{
    return map.getMap(fileName);
}

bool Mission::getConfig()
{
    return config.getConfig(fileName);
}

bool Mission::createLog()
{
    logger = new XmlLogger();
    logger->loglevel = config.SearchParams[CN_SP_LL];
    return logger -> getLog (fileName, config.LogParams);
}

void Mission::createEnvironmentOptions()
{
    options.metrictype = config.SearchParams[CN_SP_MT];
    options.allowdiagonal = config.SearchParams[CN_SP_AD];
    options.allowsqueeze = config.SearchParams[CN_SP_AS];
    options.linecost = config.SearchParams[CN_SP_LC];
    options.diagonalcost = config.SearchParams[CN_SP_DC];
    options.useresetparent = config.SearchParams[CN_SP_RP];
}

void Mission::createSearch()
{
    if (config.SearchParams[CN_SP_ST] == CN_SP_ST_ASTAR)
        search = new Astar(config.SearchParams[CN_SP_HW], config.SearchParams[CN_SP_BT], config.SearchParams[CN_SP_SL]);
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_JP_SEARCH)
        search = new JPSearch(config.SearchParams[CN_SP_HW], config.SearchParams[CN_SP_BT], config.SearchParams[CN_SP_SL]);
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_TH)
        search = new ThetaStar(config.SearchParams[CN_SP_HW], config.SearchParams[CN_SP_BT], config.SearchParams[CN_SP_SL]);
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_RSTAR)
        search = new Rstar(config.SearchParams[CN_SP_HW], config.SearchParams[CN_SP_BT],
                               config.SearchParams[CN_SP_SL], config.SearchParams[CN_SP_D2S],
                               config.SearchParams[CN_SP_NOFS], config.SearchParams[CN_SP_LSSL]);
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_RJPS)
        search = new RJPSearch(config.SearchParams[CN_SP_HW], config.SearchParams[CN_SP_BT],
                               config.SearchParams[CN_SP_SL], config.SearchParams[CN_SP_D2S],
                               config.SearchParams[CN_SP_NOFS], config.SearchParams[CN_SP_LSSL]);
}

void Mission::startSearch()
{
    sr = search->startSearch(logger, map, options);
}

void Mission::printSearchResultsToConsole()
{

        std::cout << "Path ";
        if (!sr.pathfound)
            std::cout << "NOT ";
        std::cout << "found!" << std::endl;

        std::cout << "numberofsteps=" << sr.numberofsteps << std::endl;
        std::cout << "nodescreated=" << sr.nodescreated << std::endl;

        if (sr.pathfound)
            std::cout << "pathlength=" << sr.pathlength << std::endl;
        std::cout << "time=" << sr.time << std::endl;

}

void Mission::saveSearchResultsToLog(){
    //������ - ��� ���������� ������ ��� ��� ��� ���
    logger->writeToLogSummary(sr.numberofsteps, sr.nodescreated, sr.pathlength, sr.time);

    if (sr.pathfound)
    {
        logger->writeToLogPath(*sr.lppath);
        logger->writeToLogHPpath(*sr.hppath);
        logger->writeToLogMap(map,*sr.lppath);
    }
    else
        logger->writeToLogNotFound();
    logger->saveLog();
}

const char* Mission::getAlgorithmName ()
{
    if (config.SearchParams[CN_SP_ST] == CN_SP_ST_ASTAR)
    {
        return CNS_SP_ST_ASTAR;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_DIJK)
    {
        return CNS_SP_ST_DIJK;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_BFS)
    {
        return CNS_SP_ST_BFS;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_JP_SEARCH)
    {
        return CNS_SP_ST_JP_SEARCH;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_TH)
    {
        return CNS_SP_ST_TH;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_RSTAR)
    {
        return CNS_SP_ST_RSTAR;
    }
    else if (config.SearchParams[CN_SP_ST] == CN_SP_ST_RJPS) {
        return CNS_SP_ST_RJPS;
    }
    else
    {
        return "";
    }
}
