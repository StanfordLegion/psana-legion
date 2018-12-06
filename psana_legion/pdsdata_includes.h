#include <stddef.h>
#include <stdint.h>
#include <time.h>

namespace wrapper {

// pdsdata/xtc/ClockTime.hh
#ifndef Pds_ClockTime_hh
#define Pds_ClockTime_hh

namespace Pds {
  class ClockTime {
  public:
    ClockTime();
    ClockTime(const ClockTime& t);
    ClockTime(const ::timespec& ts);
    ClockTime(const double sec);
    ClockTime(unsigned sec, unsigned nsec);

  public:
    unsigned seconds    () const {return _high;}
    unsigned nanoseconds() const {return _low;}
    double asDouble() const;
    bool isZero() const;

  public:
    ClockTime& operator=(const ClockTime&);
    bool operator> (const ClockTime&) const; 
    bool operator==(const ClockTime&) const; 

  private:
    uint32_t _low;
    uint32_t _high;
  };
}
#endif


// pdsdata/xtc/TimeStamp.hh
#ifndef PDS_TIMESTAMP_HH
#define PDS_TIMESTAMP_HH

namespace Pds {
  class TimeStamp {
  public:
    enum {NumFiducialBits = 17};
    enum {MaxFiducials = (1<<17)-32};
    enum {ErrFiducial = (1<<17)-1};
  public:
    TimeStamp();
    TimeStamp(const TimeStamp&);
    TimeStamp(const TimeStamp&, unsigned control);
    TimeStamp(unsigned ticks, unsigned fiducials, unsigned vector, unsigned control=0);

  public:
    unsigned ticks    () const;  // 119MHz counter within the fiducial for
                                 //   eventcode which initiated the readout
    unsigned fiducials() const;  // 360Hz pulse ID
    unsigned control  () const;  // internal bits for alternate interpretation
                                 //   of XTC header fields
    unsigned vector   () const;  // 15-bit seed for event-level distribution
                                 //   ( events since configure )
  public:
    TimeStamp& operator= (const TimeStamp&);
    bool       operator==(const TimeStamp&) const;
    bool       operator>=(const TimeStamp&) const;
    bool       operator<=(const TimeStamp&) const;
    bool       operator< (const TimeStamp&) const;
    bool       operator> (const TimeStamp&) const;

  private:
    uint32_t _low;
    uint32_t _high;
  };
}

#endif


// pdsdata/xtc/TransitionId.hh
#ifndef Pds_TransitionId_hh
#define Pds_TransitionId_hh

namespace Pds {

  class TransitionId {
  public:
    enum Value {
      Unknown, Reset,
      Map, Unmap,
      Configure, Unconfigure,
      BeginRun, EndRun,
      BeginCalibCycle, EndCalibCycle,
      Enable, Disable,
      L1Accept,
      NumberOf };
    static const char* name(TransitionId::Value id);
  };
}

#endif


// pdsdata/xtc/Sequence.hh
#ifndef PDS_SEQUENCE_HH
#define PDS_SEQUENCE_HH

// #include "pdsdata/xtc/ClockTime.hh"
// #include "pdsdata/xtc/TimeStamp.hh"
// #include "pdsdata/xtc/TransitionId.hh"

namespace Pds {
  class Sequence {
  public:
    enum Type    {Event = 0, Occurrence = 1, Marker = 2};
    enum         {NumberOfTypes = 3};

  public:
    Sequence() {}
    Sequence(const Sequence&);
    Sequence(const ClockTime& clock, const TimeStamp& stamp);
    Sequence(Type, TransitionId::Value, const ClockTime&, const TimeStamp&);

  public:
    Type type() const;
    TransitionId::Value  service() const;
    bool isExtended() const;
    bool isEvent() const;

  public:
    const ClockTime& clock() const {return _clock;}
    const TimeStamp& stamp() const {return _stamp;}

  public:
    Sequence& operator=(const Sequence&);

  private:
    ClockTime _clock;
    TimeStamp _stamp;
  };
}

#endif


// pdsdata/xtc/Env.h
#ifndef PDS_ENV
#define PDS_ENV

namespace Pds {
class Env 
  {
  public: 
    Env() {}
    Env(const Env& in) : _env(in._env) {}
    Env(uint32_t env);
    uint32_t value() const;
    
    const Env& operator=(const Env& that); 
  private:  
    uint32_t _env;
  };
}

inline const wrapper::Pds::Env& wrapper::Pds::Env::operator=(const wrapper::Pds::Env& that){
  _env = that._env;
  return *this;
} 

inline wrapper::Pds::Env::Env(uint32_t env) : _env(env)
  {
  }

inline uint32_t wrapper::Pds::Env::value() const
  {
  return _env;
  }

#endif


// pdsdata/xtc/TypeId.hh
#ifndef Pds_TypeId_hh
#define Pds_TypeId_hh

namespace Pds {

  class TypeId {
  public:
    /*
     * Notice: New enum values should be appended to the end of the enum list, since
     *   the old values have already been recorded in the existing xtc files.
     */
    enum Type {
      Any,
      Id_Xtc,          // generic hierarchical container
      Id_Frame,        // raw image
      Id_AcqWaveform,
      Id_AcqConfig,
      Id_TwoDGaussian, // 2-D Gaussian + covariances
      Id_Opal1kConfig,
      Id_FrameFexConfig,
      Id_EvrConfig,
      Id_TM6740Config,
      Id_ControlConfig,
      Id_pnCCDframe,
      Id_pnCCDconfig,
      Id_Epics,        // Epics Data Type
      Id_FEEGasDetEnergy,
      Id_EBeam,
      Id_PhaseCavity,
      Id_PrincetonFrame,
      Id_PrincetonConfig,
      Id_EvrData,
      Id_FrameFccdConfig,
      Id_FccdConfig,
      Id_IpimbData,
      Id_IpimbConfig,
      Id_EncoderData,
      Id_EncoderConfig,
      Id_EvrIOConfig,
      Id_PrincetonInfo,
      Id_CspadElement,
      Id_CspadConfig,
      Id_IpmFexConfig,  // LUSI Diagnostics
      Id_IpmFex,
      Id_DiodeFexConfig,
      Id_DiodeFex,
      Id_PimImageConfig,
      Id_SharedIpimb,
      Id_AcqTdcConfig,
      Id_AcqTdcData,
      Id_Index,
      Id_XampsConfig,
      Id_XampsElement,
      Id_Cspad2x2Element,
      Id_SharedPim,
      Id_Cspad2x2Config,
      Id_FexampConfig,
      Id_FexampElement,
      Id_Gsc16aiConfig,
      Id_Gsc16aiData,
      Id_PhasicsConfig,
      Id_TimepixConfig,
      Id_TimepixData,
      Id_CspadCompressedElement,
      Id_OceanOpticsConfig,
      Id_OceanOpticsData,
      Id_EpicsConfig,
      Id_FliConfig,
      Id_FliFrame,
      Id_QuartzConfig,
      Reserved1,        // previously Id_CompressedFrame        : no corresponding class
      Reserved2,        // previously Id_CompressedTimePixFrame : no corresponding class
      Id_AndorConfig,
      Id_AndorFrame,
      Id_UsdUsbData,
      Id_UsdUsbConfig,
      Id_GMD,
      Id_SharedAcqADC,
      Id_OrcaConfig,
      Id_ImpData,
      Id_ImpConfig,
      Id_AliasConfig,
      Id_L3TConfig,
      Id_L3TData,
      Id_Spectrometer,
      Id_RayonixConfig,
      Id_EpixConfig,
      Id_EpixElement,
      Id_EpixSamplerConfig,
      Id_EpixSamplerElement,
      Id_EvsConfig,
      Id_PartitionConfig,
      Id_PimaxConfig,
      Id_PimaxFrame,
      Id_Arraychar,
      Id_Epix10kConfig,
      Id_Epix100aConfig,
      Id_GenericPgpConfig,
      Id_TimeToolConfig,
      Id_TimeToolData,
      Id_EpixSConfig,
      Id_SmlDataConfig,
      Id_SmlDataOrigDgramOffset,
      Id_SmlDataProxy,
      Id_ArrayUInt16,
      Id_GotthardConfig,
      Id_AnalogInput,
      Id_SmlData,
      Id_Andor3dConfig,
      Id_Andor3dFrame,
      Id_BeamMonitorBldData,
      Id_Generic1DConfig,
      Id_Generic1DData,
      Id_UsdUsbFexConfig,
      Id_UsdUsbFexData,
      Id_EOrbits,
      Id_SharedUsdUsb,
      Id_ControlsCameraConfig,
      Id_ArchonConfig,
      Id_JungfrauConfig,
      Id_JungfrauElement,
      Id_QuadAdcConfig,
      Id_ZylaConfig,
      Id_ZylaFrame,
      Id_Epix10kaConfig,
      NumberOf};
    enum { VCompressed = 0x8000 };

    TypeId() {}
    TypeId(const TypeId& v);
    TypeId(Type type, uint32_t version, bool compressed=false);
    TypeId(const char*);

    Type     id()      const;
    uint32_t version() const;
    uint32_t value()   const;

    bool     compressed() const;
    unsigned compressed_version() const;

    bool     is_configuration() const;

    static const char* name(Type type);
    static uint32_t _sizeof() { return sizeof(TypeId); }

  private:
    uint32_t _value;
  };

}

#endif


// pdsdata/xtc/Level.hh
#ifndef PDSLEVEL_HH
#define PDSLEVEL_HH

namespace Pds {
class Level {
public:
  enum Type{Control, Source, Segment, Event, Recorder, Observer, Reporter,
            NumberOfLevels};
  static const char* name(Type type);
};
}

#endif

// pdsdata/xtc/Src.hh
#ifndef Pds_Src_hh
#define Pds_Src_hh

// #include "pdsdata/xtc/Level.hh"

namespace Pds {

  class Node;

  class Src {
  public:

    Src();
    Src(Level::Type level);

    uint32_t log()   const;
    uint32_t phy()   const;

    Level::Type level() const;

    bool operator==(const Src& s) const;
    bool operator<(const Src& s) const;

    static uint32_t _sizeof() { return sizeof(Src); }
  protected:
    uint32_t _log; // logical  identifier
    uint32_t _phy; // physical identifier
  };

}
#endif


// pdsdata/xtc/Damage.hh
#ifndef Pds_Damage_hh
#define Pds_Damage_hh

namespace Pds {

  class Damage {
  public:
    enum Value {
      DroppedContribution    = 1,
      Uninitialized          = 11,
      OutOfOrder             = 12,
      OutOfSynch             = 13,
      UserDefined            = 14,
      IncompleteContribution = 15,
      ContainsIncomplete     = 16
    };
    // reserve the top byte to augment user defined errors
    enum {NotUserBitsMask=0x00FFFFFF, UserBitsShift = 24};

    Damage() {}
    Damage(uint32_t v) : _damage(v) {}
    uint32_t  value() const             { return _damage; }
    void     increase(Damage::Value v)  { _damage |= ((1<<v) & NotUserBitsMask); }
    void     increase(uint32_t v)       { _damage |= v & NotUserBitsMask; }
    uint32_t bits() const               { return _damage & NotUserBitsMask;}
    uint32_t userBits() const           { return _damage >> UserBitsShift; }
    void     userBits(uint32_t v) {
      _damage &= NotUserBitsMask;
      _damage |= (v << UserBitsShift);
    }
    
  private:
    uint32_t _damage;
  };
}

#endif


// pdsdata/xtc/Xtc.hh
#ifndef Pds_Xtc_hh
#define Pds_Xtc_hh

// #include "pdsdata/xtc/TypeId.hh"
// #include "Damage.hh"
// #include "Src.hh"

namespace Pds {

  class Xtc {
  public:
    Xtc() : damage(0), extent(0) {};
    Xtc(const Xtc& xtc) :
      damage(xtc.damage), src(xtc.src), contains(xtc.contains), extent(sizeof(Xtc)) {}
    Xtc(const TypeId& type) : 
      damage(0), contains(type), extent(sizeof(Xtc)) {}
    Xtc(const TypeId& type, const Src& _src) : 
      damage(0), src(_src), contains(type), extent(sizeof(Xtc)) {}
    Xtc(const TypeId& _tag, const Src& _src, unsigned _damage) : 
      damage(_damage), src(_src), contains(_tag), extent(sizeof(Xtc)) {}
    Xtc(const TypeId& _tag, const Src& _src, const Damage& _damage) : damage(_damage), src(_src), contains(_tag), extent(sizeof(Xtc)) {}
    
    void* operator new(size_t size, char* p)     { return (void*)p; }
    void* operator new(size_t size, Xtc* p)      { return p->alloc(size); }
    
    char*        payload()       const { return (char*)(this+1); }
    int          sizeofPayload() const { return extent - sizeof(Xtc); }
    Xtc*       next()                { return (Xtc*)((char*)this+extent); }
    const Xtc* next()          const { return (const Xtc*)((char*)this+extent); }
    
    void*        alloc(uint32_t size)  { void* buffer = next(); extent += size; return buffer; }

    Damage   damage;
    Src      src;
    TypeId   contains;
    uint32_t extent;
  };
}

#endif


// pdsdata/xtc/Dgram.hh
#ifndef Pds_Dgram_hh
#define Pds_Dgram_hh

// #include "Sequence.hh"
// #include "Env.hh"
// #include "Xtc.hh"

namespace Pds {

#define PDS_DGRAM_STRUCT Sequence seq; Env env; Xtc xtc

  class Dgram {
  public:
    PDS_DGRAM_STRUCT;
  };

}
#endif

}
