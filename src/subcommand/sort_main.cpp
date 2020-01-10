#include "subcommand.hpp"
#include "odgi.hpp"
#include "args.hxx"
#include "algorithms/topological_sort.hpp"
#include "algorithms/eades_algorithm.hpp"
#include "algorithms/cycle_breaking_sort.hpp"
#include "algorithms/id_ordered_paths.hpp"
#include "algorithms/dagify.hpp"
#include "algorithms/split_strands.hpp"
#include "algorithms/dagify_sort.hpp"
#include "algorithms/random_order.hpp"
#include "algorithms/mondriaan_sort.hpp"

namespace odgi {

using namespace odgi::subcommand;

int main_sort(int argc, char** argv) {

    // trick argumentparser to do the right thing with the subcommand
    for (uint64_t i = 1; i < argc-1; ++i) {
        argv[i] = argv[i+1];
    }
    std::string prog_name = "odgi sort";
    argv[0] = (char*)prog_name.c_str();
    --argc;
    
    args::ArgumentParser parser("variation graph sorts");
    args::HelpFlag help(parser, "help", "display this help summary", {'h', "help"});
    args::ValueFlag<std::string> dg_out_file(parser, "FILE", "store the graph in this file", {'o', "out"});
    args::ValueFlag<std::string> dg_in_file(parser, "FILE", "load the graph from this file", {'i', "idx"});
    args::Flag show_sort(parser, "show", "write the sort order mapping", {'S', "show"});
    args::ValueFlag<std::string> sort_order_in(parser, "FILE", "load the sort order from this file", {'s', "sort-order"});
    args::Flag cycle_breaking(parser, "cycle_breaking", "use a cycle breaking sort", {'c', "cycle-breaking"});
    args::Flag breadth_first(parser, "breadth_first", "use a breadth first topological sort", {'b', "breadth-first"});
    args::Flag dagify(parser, "dagify", "sort on the basis of the DAGified graph", {'d', "dagify-sort"});
    args::Flag eades(parser, "eades", "use eades algorithm", {'e', "eades"});
    args::Flag lazy(parser, "lazy", "use lazy topological algorithm (DAG only)", {'l', "lazy"});
    args::Flag two(parser, "two", "use two-way (max of head-first and tail-first) topological algorithm", {'w', "two-way"});
    args::Flag randomize(parser, "random", "randomly sort the graph", {'r', "random"});
    args::Flag no_seeds(parser, "no-seeds", "don't use heads or tails to seed topological sort", {'n', "no-seeds"});
    args::Flag mondriaan(parser, "mondriaan", "use sparse matrix diagonalization to sort the graph", {'m', "mondriaan"});
    args::ValueFlag<uint64_t> mondriaan_n_parts(parser, "N", "number of partitions for mondriaan", {'N', "mondriaan-n-parts"});
    args::ValueFlag<double> mondriaan_epsilon(parser, "N", "epsilon parameter to mondriaan", {'E', "mondriaan-epsilon"});
    args::Flag mondriaan_path_weight(parser, "path-weight", "weight mondriaan input matrix by path coverage of edges", {'W', "mondriaan-path-weight"});
    args::ValueFlag<std::string> pipeline(parser, "STRING", "apply a series of sorts, based on single-character command line arguments to this command, with 's' the default sort and 'f' to reverse the sort order", {'p', "pipeline"});
    args::Flag paths_by_min_node_id(parser, "paths-min", "sort paths by their lowest contained node id", {'L', "paths-min"});
    args::Flag paths_by_max_node_id(parser, "paths-max", "sort paths by their highest contained node id", {'M', "paths-max"});
    args::Flag paths_by_avg_node_id(parser, "paths-avg", "sort paths by their average contained node id", {'A', "paths-avg"});
    args::Flag paths_by_avg_node_id_rev(parser, "paths-avg-rev", "sort paths in reverse by their average contained node id", {'R', "paths-avg-rev"});
    args::ValueFlag<std::string> path_delim(parser, "path-delim", "sort paths in bins by their prefix up to this delemiter", {'D', "path-delim"});
    args::Flag progress(parser, "progress", "display progress of the sort", {'P', "progress"});
    args::Flag optimize(parser, "optimize", "use the MutableHandleGraph::optimize method", {'O', "optimize"});
    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    if (argc==1) {
        std::cout << parser;
        return 1;
    }

    graph_t graph;
    assert(argc > 0);
    std::string infile = args::get(dg_in_file);
    if (infile.size()) {
        if (infile == "-") {
            graph.load(std::cin);
        } else {
            ifstream f(infile.c_str());
            graph.load(f);
            f.close();
        }
    }
    if (args::get(show_sort)) {
        std::vector<handle_t> order = (args::get(lazy) ? algorithms::lazy_topological_order(&graph) : algorithms::topological_order(&graph));
        for (auto& handle : order) {
            std::cout << graph.get_id(handle) << std::endl;
        }
    }

    // helper, TODO: move into its own file
    // make a dagified copy, get its sort, and apply the order to our graph

    std::string outfile = args::get(dg_out_file);
    if (outfile.size()) {
        if (args::get(eades)) {
            graph.apply_ordering(algorithms::eades_algorithm(&graph), true);
        } else if (args::get(lazy)) {
            graph.apply_ordering(algorithms::lazy_topological_order(&graph), true);
        } else if (args::get(two)) {
            graph.apply_ordering(algorithms::two_way_topological_order(&graph), true);
        } else if (args::get(optimize)) {
            graph.optimize();
        } else if (!args::get(sort_order_in).empty()) {
            std::vector<handle_t> given_order;
            std::string buf;
            std::ifstream in_order(args::get(sort_order_in).c_str());
            while (std::getline(in_order, buf)) {
                given_order.push_back(graph.get_handle(std::stol(buf)));
            }
            graph.apply_ordering(given_order, true);
        } else if (args::get(dagify)) {
            graph_t split, into;
            graph.apply_ordering(algorithms::dagify_sort(graph, split, into), true);
        } else if (args::get(cycle_breaking)) {
            graph.apply_ordering(algorithms::cycle_breaking_sort(graph), true);
        } else if (args::get(no_seeds)) {
            graph.apply_ordering(algorithms::topological_order(&graph, false, false, args::get(progress)), true);
        } else if (args::get(mondriaan)) {
            graph.apply_ordering(algorithms::mondriaan_sort(graph,
                                                            args::get(mondriaan_n_parts),
                                                            args::get(mondriaan_epsilon),
                                                            args::get(mondriaan_path_weight), false), true);
        } else if (args::get(breadth_first)) {
            graph.apply_ordering(algorithms::breadth_first_topological_order(graph), true);
        } else if (args::get(randomize)) {
            graph.apply_ordering(algorithms::random_order(graph), true);
        } else if (!args::get(pipeline).empty()) {
            // for each sort type, apply it to the graph
            std::vector<handle_t> order;
            for (auto c : args::get(pipeline)) {
                switch (c) {
                case 's':
                    order = algorithms::topological_order(&graph, true, false, args::get(progress));
                    break;
                case 'n':
                    order = algorithms::topological_order(&graph, false, false, args::get(progress));
                    break;
                case 'e':
                    order = algorithms::eades_algorithm(&graph);
                    break;
                case 'd':
                {
                    graph_t split, into;
                    order = algorithms::dagify_sort(graph, split, into);
                }
                    break;
                case 'c':
                    order = algorithms::cycle_breaking_sort(graph);
                    break;
                case 'b':
                    order = algorithms::breadth_first_topological_order(graph);
                    break;
                case 'l':
                    order = algorithms::lazy_topological_order(&graph);
                    break;
                case 'w':
                    order = algorithms::two_way_topological_order(&graph);
                    break;
                case 'r':
                    order = algorithms::random_order(graph);
                    break;
                case 'f':
                    order.clear();
                    graph.for_each_handle([&order](const handle_t& handle) {
                            order.push_back(handle);
                        });
                    std::reverse(order.begin(), order.end());
                    break;
                case 'm':

                    order = algorithms::mondriaan_sort(graph,
                                                       args::get(mondriaan_n_parts),
                                                       args::get(mondriaan_epsilon),
                                                       args::get(mondriaan_path_weight), false);
                    break;
                default:
                    break;
                }
                graph.apply_ordering(order, true);
            }
        } else {
            graph.apply_ordering(algorithms::topological_order(&graph, true, false, args::get(progress)), true);
        }
        if (args::get(paths_by_min_node_id)) {
            graph.apply_path_ordering(algorithms::prefix_and_id_ordered_paths(graph, args::get(path_delim), false, false));
        }
        if (args::get(paths_by_max_node_id)) {
            graph.apply_path_ordering(algorithms::prefix_and_id_ordered_paths(graph, args::get(path_delim), false, true));
        }
        if (args::get(paths_by_avg_node_id)) {
            graph.apply_path_ordering(algorithms::prefix_and_id_ordered_paths(graph, args::get(path_delim), true, false));
        }
        if (args::get(paths_by_avg_node_id_rev)) {
            graph.apply_path_ordering(algorithms::prefix_and_id_ordered_paths(graph, args::get(path_delim), true, true));
        }
        if (outfile == "-") {
            graph.serialize(std::cout);
        } else {
            ofstream f(outfile.c_str());
            graph.serialize(f);
            f.close();
        }
    }
    return 0;
}

static Subcommand odgi_build("sort", "topologically order the graph",
                              PIPELINE, 3, main_sort);


}
