#pragma once

#include "Engine/EngineCoreAPI.h"
#include "core/features/Serializable.h"
#include "core/features/MathJson.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

template<typename T>
concept ReflectedSettings = std::default_initializable<T> && requires { T::GetStaticType(); };

// one registered settings struct
struct GraphicsSettingsEntry {
	std::type_index type;
	std::string name;				// display name the renderer registered it under
	std::shared_ptr<void> data;
};

// readOnly fields are written by the renderer every frame and buttons are one frame requests, neither is a setting
inline bool IsPersistentField(const FieldMeta& meta)
{
	return !meta.readOnly && meta.widget != FieldWidget::Button;
}

// json of one settings type, works on the raw pointer of the entry like SerializerInternal does for components
template<typename Type>
struct GraphicsSettingsSerializerInternal {
	static nlohmann::json Serialize(const void* instance) {
		nlohmann::json j = nlohmann::json::object();
		if (!instance) {
			return j;
		}
		const Type& settings = *static_cast<const Type*>(instance);
		Type::VisitFields([&](const char* name, auto memberPtr, auto&&... meta) {
			if (IsPersistentField(ResolveFieldMeta(meta...))) {
				j[name] = settings.*memberPtr;
			}
		});
		return j;
	}

	// fields missing from the json keep their current value
	static void Deserialize(void* instance, const nlohmann::json& j) {
		Type& settings = *static_cast<Type*>(instance);
		Type::VisitFields([&](const char* name, auto memberPtr, auto&&... meta) {
			if (IsPersistentField(ResolveFieldMeta(meta...)) && j.contains(name)) {
				j.at(name).get_to(settings.*memberPtr);
			}
		});
	}
};

// Owns one instance of each registered settings struct (GraphicsSettings.h), shared by the renderer that
// registered it and the editor. Instances keep their address until removed, renderers hold a pointer to theirs.
// The editor draws an entry from its type and data, the way the lua bindings resolve components.
//
// settings json layout:
//   { "graphics_settings": { "<Type>": { <persistent fields> } } }
class MYGRAPHICSENGINE_EXPORT GraphicsSettingsSerializer : public Serializable
{
public:
	using Entry = GraphicsSettingsEntry;

	static constexpr const char* DefaultPath = "assets/data/graphics_settings.json";

	GraphicsSettingsSerializer();
	~GraphicsSettingsSerializer() = default;

	static void RegisterSettings();

	// registers T under a display name, or returns the instance already registered.
	// a new instance starts with the values loaded for its type, if any
	template<ReflectedSettings T>
	T& add(std::string_view name) {
		if (T* existing = get<T>()) {
			return *existing;
		}
		auto data = std::make_shared<T>();
		T& settings = *data;
		m_entries.push_back(Entry{ typeid(T), std::string(name), std::move(data) });
		_applyLoaded(m_entries.back());
		return settings;
	}

	template<ReflectedSettings T>
	void remove() {
		std::erase_if(m_entries, [](const Entry& entry) { return entry.type == typeid(T); });
	}

	// nullptr when the renderer owning T is not running
	template<ReflectedSettings T>
	T* get() const {
		auto it = std::find_if(m_entries.begin(), m_entries.end(), [](const Entry& entry) { return entry.type == typeid(T); });
		return it != m_entries.end() ? static_cast<T*>(it->data.get()) : nullptr;
	}

	const std::vector<Entry>& entries() const { return m_entries; }

	bool saveSettings(std::string_view path) const;
	bool loadSettings(std::string_view path);

	virtual void serialize(nlohmann::json& j) const override;
	virtual void deserialize(const nlohmann::json& j) override;

private:
	using SettingsSaver = nlohmann::json(*)(const void*);
	using SettingsLoader = void(*)(void*, const nlohmann::json&);

	struct SettingsHandler {
		std::string typeName;
		SettingsSaver save;
		SettingsLoader load;
	};

	static const SettingsHandler* _findHandler(std::type_index type);
	// gives an entry the values loaded for its type
	void _applyLoaded(Entry& entry) const;

	std::vector<Entry> m_entries;
	nlohmann::json m_loaded = nlohmann::json::object();

	inline static std::unordered_map<std::type_index, SettingsHandler> settings_handlers;
};
