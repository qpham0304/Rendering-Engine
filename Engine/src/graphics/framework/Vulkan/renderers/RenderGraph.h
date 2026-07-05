// #include <iostream>
// #include <unordered_map>
// #include <unordered_set>
// #include <memory>
// #include <algorithm>

// #include <string>
// #include <vector>
// #include <string_view>

// struct RenderPassNode {
//     std::string name;
//     std::vector<std::string> reads;  // Inputs / Dependencies
//     std::vector<std::string> writes; // Outputs / What this pass produces
    
//     // Virtual or function pointer execution
//     virtual void init() = 0;
//     virtual void render() = 0;
//     virtual ~RenderPassNode() = default;
// };

// class RenderGraph {
// public:
//     void addPass(std::shared_ptr<RenderPassNode> pass) {
//         m_passes[pass->name] = pass;
//     }

//     bool compile() {
//         m_sortedPasses.clear();
//         std::unordered_map<std::string, std::vector<std::string>> adjacencyList;
//         std::unordered_map<std::string, std::string> resourceProducers;

//         // map which pass writes which resource
//         for (const auto& [name, pass] : m_passes) {
//             for (const auto& output : pass->writes) {
//                 resourceProducers[output] = name;
//             }
//         }

//         // build the adjacency list (Dependency Graph)
//         for (const auto& [name, pass] : m_passes) {
//             for (const auto& input : pass->reads) {
//                 if (resourceProducers.contains(input)) {
//                     std::string dependency = resourceProducers[input];
//                     adjacencyList[dependency].push_back(name);  // 'dependency' must execute BEFORE 'name'
//                 }
//             }
//         }

//         // topological Sort via DFS
//         std::unordered_set<std::string> visited;
//         std::unordered_set<std::string> visiting; // For cycle detection

//         for (const auto& [name, _] : m_passes) {
//             if (!visited.contains(name)) {
//                 if (!dfs(name, adjacencyList, visited, visiting)) {
//                     std::cerr << "CRITICAL: Dependency cycle detected in Render Graph!\n";
//                     return false;
//                 }
//             }
//         }

//         // flip the reversed order from DFS
//         std::reverse(m_sortedPasses.begin(), m_sortedPasses.end());
//         return true;
//     }

//     const std::vector<std::shared_ptr<RenderPassNode>>& getExecutionOrder() const {
//         return m_sortedPasses;
//     }

// private:
//     bool dfs(const std::string& node, 
//              std::unordered_map<std::string, std::vector<std::string>>& adj,
//              std::unordered_set<std::string>& visited, 
//              std::unordered_set<std::string>& visiting) 
//     {
//         visiting.insert(node);

//         for (const auto& neighbor : adj[node]) {
//             if (visiting.contains(neighbor)) return false; // Cycle!
//             if (!visited.contains(neighbor)) {
//                 if (!dfs(neighbor, adj, visited, visiting)) return false;
//             }
//         }

//         visiting.erase(node);
//         visited.insert(node);
//         m_sortedPasses.push_back(m_passes[node]);
//         return true;
//     }

//     std::unordered_map<std::string, std::shared_ptr<RenderPassNode>> m_passes;
//     std::vector<std::shared_ptr<RenderPassNode>> m_sortedPasses;
// };