#ifndef PARAMETER_HANDLER_H
#define PARAMETER_HANDLER_H


#include <misc/definitions.h>


namespace GCSA
{
typedef CSA::usint usint;

class ParameterHandler
{
  public:
    ParameterHandler(int argc, char** argv, bool _rlcsa, std::string _usage);

    void printUsage() const;
    void printOptions() const;

    bool ok, rlcsa;
    std::string usage;

    bool  indels, locate, penalties, reverse_complement, verbose, write, binary_patterns, detailed;
    char* index_name;
    char* patterns_name;
    usint k, skip, max_matches, begin = 0, end = 0;
};

};  // namespace CSA


#endif  // PARAMETER_HANDLER_H
