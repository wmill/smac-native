#include "smac/core/game_state.hpp"
#include "smac/core/pathfinding.hpp"
#include "smac/formats/data_directory.hpp"
#include "smac/formats/rules.hpp"
#include "smac/formats/terran_map.hpp"
#include "smac/formats/text.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <variant>
static int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<__FILE__<<':'<<__LINE__<<": check failed: " #x "\n";++failures;}}while(false)
static void put32(std::vector<std::byte>&b,std::size_t o,std::uint32_t v){for(int i=0;i<4;++i)b[o+static_cast<std::size_t>(i)]=static_cast<std::byte>((v>>(i*8))&255U);}
int main(){namespace sc=smac::core;namespace sf=smac::formats;
  sc::WorldMap map(8,4,true);CHECK(map.valid({0,0}));CHECK(!map.valid({1,0}));CHECK((map.normalize({-2,0})==sc::MapPosition{6,0}));CHECK(map.neighbors({0,0}).size()==4);CHECK(map.neighbors({1,1}).size()==6);
  for(auto&t:map.tiles())t.terrain=sc::Terrain::land;sc::GameState state(std::move(map));state.units().push_back({7,1,{0,0},3,3});auto events=state.apply(sc::MoveUnit{7,{6,0}});CHECK(std::holds_alternative<sc::UnitMoved>(events[0]));CHECK(state.units()[0].movement_remaining==0);CHECK(std::holds_alternative<sc::CommandRejected>(state.apply(sc::MoveUnit{7,{4,0}})[0]));state.apply(sc::EndTurn{});CHECK(state.turn()==2&&state.units()[0].movement_remaining==3);auto hash=state.stable_hash();CHECK(hash==state.stable_hash());
  sc::WorldMap pathmap(8,4,true);for(auto&t:pathmap.tiles())t.terrain=sc::Terrain::land;CHECK(!sc::find_path(pathmap,{0,0},{2,0},3).empty());CHECK(sc::find_path(pathmap,{0,0},{4,0},3).empty());
  auto rules=sf::parse_rules("; hello\r\n#RULES\r\n3, ; roads\r\n## translator comment\r\nnot data\r\n#THING\r\nName, 2\r\n# ; end\r\n");CHECK(sf::ok(rules));CHECK(std::get<sf::ParsedRules>(rules).database.road_movement_rate==3);CHECK(std::get<sf::ParsedRules>(rules).sections.size()==2);CHECK(sf::normalize_text(std::string("A\x97"))=="A\xE2\x80\x94");
  std::vector<std::byte> bytes(15+2724+4*44);const char magic[]="TERRANMAP";for(int i=0;i<9;++i)bytes[static_cast<std::size_t>(i)]=static_cast<std::byte>(magic[i]);put32(bytes,15,4);put32(bytes,19,2);put32(bytes,23,42);for(std::size_t i=15+2724;i<bytes.size();i+=44)bytes[i]=std::byte{0x80};auto tm=sf::parse_terran_map(bytes);CHECK(sf::ok(tm));if(sf::ok(tm)){auto&m=std::get<sf::TerranMap>(tm);CHECK(m.tiles.size()==4&&m.seed==42);CHECK(m.to_world_map().at({0,0}).terrain==sc::Terrain::land);}bytes.resize(30);CHECK(!sf::ok(sf::parse_terran_map(bytes)));
  auto root=std::filesystem::temp_directory_path()/"smac-native-case-test";std::filesystem::create_directories(root);auto f=root/"Units.PCX";{std::ofstream o(f);o<<"x";}CHECK(sf::find_case_insensitive(root,"units.pcx")==f);std::filesystem::remove(f);std::filesystem::remove(root);
  if(failures){std::cerr<<failures<<" failure(s)\n";return 1;}std::cout<<"all tests passed\n";return 0;}
