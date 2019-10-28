#ifndef STORAGE
#define STORAGE

namespace Heating
{
    class Storage
    {
        public:
            bool loadConfiguration(Config & config) const;
            bool saveConfiguration(const char * json) const;
    };
}

#endif
