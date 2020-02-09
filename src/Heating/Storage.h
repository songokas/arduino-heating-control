#ifndef HEATING_STORAGE_H
#define HEATING_STORAGE_H

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
