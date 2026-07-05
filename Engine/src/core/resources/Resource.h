#include <string>

class Resource {
public:
    bool isLoaded() {return m_loaded; }

    bool load() { 
        m_loaded = _load();
        return m_loaded;
    };

    bool unload() { 
        m_loaded = unload();
        return m_loaded;
    };

protected:
    virtual bool _load() { return false; };
    virtual bool _unload() { return false; };

private:
    bool m_loaded { false };
    std::string m_name { "None" };
};