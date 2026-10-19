#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <string_view>

#include "core/features/Texture.h"

struct RenderPassNode {
    std::string name;
    std::vector<std::shared_ptr<Texture>> reads;  // Dependencies, use member path for name/type
    std::vector<std::shared_ptr<Texture>> writes; // Outputs, use member path  for name/type
    
    virtual void init() = 0;
    virtual void render() = 0;
    virtual ~RenderPassNode() = default;
};

class RenderGraphVulkan {
public:
    RenderGraphVulkan();
    void addPass(std::shared_ptr<RenderPassNode> pass);
    bool compile();
    const std::vector<std::shared_ptr<RenderPassNode>>& getExecutionOrder() const;
    void execute();

private:
    bool dfs(
        const std::string& node, 
        std::unordered_map<std::string, std::vector<std::string>>& adj,
        std::unordered_set<std::string>& visited, 
        std::unordered_set<std::string>& visiting
    );

    std::unordered_map<std::string, std::shared_ptr<RenderPassNode>> m_passes;
    std::vector<std::shared_ptr<RenderPassNode>> m_sortedPasses;
};