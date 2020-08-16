#pragma once
#ifndef INC_HELPERS_H_
#define INC_HELPERS_H_
#include <vector>
#include <tuple>
class SendApplication;
std::tuple<unsigned, unsigned, unsigned> checkApplicationPerformance(const std::vector<SendApplication>& applications);
bool isPerformingWell (const SendApplication& app);
bool isPerformingBad (const SendApplication& app);
#endif