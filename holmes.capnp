@0xaaef86128cdda946;
using Cxx = import "/capnp/c++.capnp";

$Cxx.namespace("holmes");

interface Holmes {
  # Dynamic value type for use in facts
  struct Val {
    union {
      stringVal @0 :Text;
      addrVal   @1 :UInt64;
      blobVal   @2 :Data;
      jsonVal   @3 :Text;
    }
  }
  
  # Type of a dynamic variable
  enum HType {
    string @0;
    addr   @1;
    blob   @2;
    json   @3;
  }

  # Variables
  using Var = UInt32;

  # Logical facts
  using FactName = Text;
  struct Fact {
    factName @0 :FactName;
    args     @1 :List(Val);
  }

  # Argument restriction when searching
  struct TemplateVal {
    union {
      exactVal @0 :Val;  #Argument must have this exact value
      unbound  @1 :Void; #Argument is unrestricted
      bound    @2 :Var;  #Argument is bound to a var and must be consistent
    }
  }

  # FactTemplate to be used as a search query
  # Variables _must_ be used from 0+ sequentially.
  struct FactTemplate {
    factName @0 :FactName;
    args     @1 :List(TemplateVal);
  }
  
  # Callback provided by an analysis
  interface Analysis {
    analyze @0 (context :List(Val)) -> (derived :List(Fact));
  }

  # Assert a fact to the server
  set @0 (facts :List(Fact));
  
  # Ask the server to search for facts
  derive @1 (target :List(FactTemplate)) -> (ctx :List(List(Val)));
  
  # Register as an analysis
  analyzer @2 (name        :Text,
               premises    :List(FactTemplate),
	       analysis    :Analysis);

  # Register a fact type
  # If it's not present, inform the DAL
  # If it is present, check compatibility
  registerType @3 (factName :Text,
                   argTypes :List(HType)) -> (valid :Bool);
}
