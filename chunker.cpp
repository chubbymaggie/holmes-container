#include "holmes.capnp.h"

#include <capnp/ez-rpc.h>
#include <kj/debug.h>

#include <iostream>
#include <memory>
#include <vector>
#include <assert.h>

using holmes::Holmes;
using capnp::Orphan;

#define FILENAME 0
#define BASE 1
#define CONTENTS 2

class ChunkSection final : public Holmes::Analysis::Server {
  public:
    kj::Promise<void> analyze(AnalyzeContext context) {
      auto ctx = context.getParams().getContext();
      std::string fileName(ctx[FILENAME].getStringVal());
      uint64_t base = ctx[BASE].getAddrVal();
      capnp::Data::Reader contents(ctx[CONTENTS].getBlobVal());
      auto bssMode = false;
      auto orphanage = capnp::Orphanage::getForMessageContaining(context.getResults());
      std::vector<capnp::Orphan<Holmes::Fact> > derived;
      for (size_t i = 0; i < contents.size(); i++) {
        Orphan<Holmes::Fact> fact = orphanage.newOrphan<Holmes::Fact>();
        auto fb = fact.get();
        fb.setFactName("word128");
        auto ab = fb.initArgs(3);
        ab[0].setStringVal(fileName);
        ab[1].setAddrVal(base + i);
        if (bssMode) {
          Orphan<capnp::Data> bss = orphanage.newOrphan<capnp::Data>(16);
          ab[2].adoptBlobVal(kj::mv(bss));
        } else {
          size_t end = std::min(i + 16, contents.size());
          ab[2].setBlobVal(contents.slice(i, end));
        }
        derived.push_back(kj::mv(fact));
      }
      auto derivedBuilder = context.getResults().initDerived(derived.size());
      auto i = 0;
      while (!derived.empty()) {
        derivedBuilder.adoptWithCaveats(i++, kj::mv(derived.back()));
        derived.pop_back();
      }
      return kj::READY_NOW;
    }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " HOST:PORT" << std::endl;
    return 1;
  }
  capnp::EzRpcClient client(argv[1]);
  holmes::Holmes::Client holmes = client.importCap<holmes::Holmes>("holmes");
  auto& waitScope = client.getWaitScope();
  
  auto chunkReq = holmes.registerTypeRequest();
  chunkReq.setFactName("word128");
  auto chunkArgTypes = chunkReq.initArgTypes(3);
  chunkArgTypes.set(0, holmes::Holmes::HType::STRING);
  chunkArgTypes.set(1, holmes::Holmes::HType::ADDR);
  chunkArgTypes.set(2, holmes::Holmes::HType::BLOB);
  auto chunkRes = chunkReq.send();

  auto sectReq = holmes.registerTypeRequest();
  sectReq.setFactName("section");
  auto sectReqTypes = sectReq.initArgTypes(6);
  sectReqTypes.set(0, holmes::Holmes::HType::STRING);
  sectReqTypes.set(1, holmes::Holmes::HType::STRING);
  sectReqTypes.set(2, holmes::Holmes::HType::ADDR);
  sectReqTypes.set(3, holmes::Holmes::HType::ADDR);
  sectReqTypes.set(4, holmes::Holmes::HType::BLOB);
  sectReqTypes.set(5, holmes::Holmes::HType::STRING);
  auto sectRes = sectReq.send();

  assert(chunkRes.wait(waitScope).getValid());
  assert(sectRes.wait(waitScope).getValid());

  auto request = holmes.analyzerRequest();
  auto prems = request.initPremises(1);
  prems[0].setFactName("section");
  auto args = prems[0].initArgs(6);
  args[0].setBound(FILENAME);
  args[1].setUnbound();
  args[2].setBound(BASE);
  args[3].setUnbound();
  args[4].setBound(CONTENTS);
  auto ev = args[5].initExactVal();
  ev.setStringVal(".text");

  request.setAnalysis(kj::heap<ChunkSection>());

  request.setName("chunker");

  request.send().wait(waitScope);
  return 0;
}
