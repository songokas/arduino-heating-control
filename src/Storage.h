#ifndef STORAGE
#define STORAGE

namespace Heating
{
    class Storage
    {
        public:
            bool loadConfiguration(Config & config) const;
            bool saveConfiguration(Config & config, String & json) const;
    };
}

#endif
