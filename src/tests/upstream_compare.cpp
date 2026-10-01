// Built by compare_upstream.py against unmodified reference source.
#include "reference.h"
#include <iostream>
#include <stdexcept>
#include <memory>

static void require(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); }
template<class Manager> void drain(Manager& manager) {
    for(int i=0;i<64 && !manager.request_fifo.IsEmpty();++i) manager.ProcessRequests();
    require(manager.request_fifo.IsEmpty(),"Unbounded file requests");
    require(manager.debug_fifo.IsEmpty(),"File request failed");
}
template<class Reader,class Manager> void init(Reader& r,Manager& m,
        daisy::RamBufferMemory* ram,const std::vector<int16_t>& pcm) {
    m.Init(48000);
    // A valid file during initialization avoids the original empty-file unsigned
    // subtraction. The port explicitly guards that case instead.
    r.fptr_read.pcm=pcm.data(); r.fptr_read.count=pcm.size();
    daisy::f_open(&r.fptr_read,"normal",0);
    r.Init(m,48000,ram);
    r.OpenFile("normal","double",true); drain(m);
    r.SetVelocity(93); r.StartPlaying(); drain(m);
}
template<class Reader> void events(Reader& r,int n) {
    if(n==7000) r.StartPlaying();
    if(n==14000) r.SetGlobalPitch(2.f);
    if(n==23000) r.SetGlobalPitch(.75f);
    if(n==33000) r.SetReverse(true);
    if(n==45000) { r.SetStartPoint(.2f); r.SetEndPoint(.8f); }
    if(n==53000) { r.SetVarispeed(3.f); r.StartPlaying(); }
    if(n==62000) { r.SetGain(.3f); r.SetPan(.9f); }
    if(n==74000) r.StopPlaying();
    if(n==80000) { r.SetSustainActive(false); r.SetDecay(.01f); r.StartPlaying(); }
    if(n==93000) { r.SetAutoLoop(false); r.SetSustainActive(true); r.SetReverse(false); r.StartPlaying(); }
}
int main() { try {
    std::vector<int16_t> pcm(24000*2);
    for(size_t i=0;i<pcm.size()/2;++i) {
        pcm[2*i]=int16_t(12000*std::sin(float(i)*.037f));
        pcm[2*i+1]=int16_t(7000*std::cos(float(i)*.063f));
    }
    auto ram=std::make_unique<nibbi::LoopStorage>();
    daisy::FileSampleReader port{}; daisy::FileStreamingManager pm{};
    reference::FileSampleReader original{}; reference::FileStreamingManager om{};
    init(port,pm,&ram->memory,pcm); init(original,om,&ram->memory,pcm);
    double energy=0;
    for(int i=0;i<140000;++i) {
        events(port,i); events(original,i); drain(pm); drain(om);
        float l=0,r=0,ol=0,orr=0;
        port.PopStereoSamps(&l,&r); original.PopStereoSamps(&ol,&orr);
        if(l!=ol || r!=orr) {
            std::cerr<<"Sampler mismatch at frame "<<i<<": "<<l<<", "<<ol<<'\n';
            throw std::runtime_error("Original sampler comparison failed");
        }
        require(std::isfinite(l)&&std::isfinite(r),"Non-finite sampler output");
        energy+=l*l+r*r; drain(pm); drain(om);
    }
    require(energy>1,"Comparison must exercise audible output");

    auto ram2=std::make_unique<nibbi::LoopStorage>();
    daisy::LooperEngine loop{};
    // Original's user-provided empty constructor relies on global zeroed storage.
    static reference::LooperEngine originalLoop;
    loop.Init(48000,&ram->memory,true); originalLoop.Init(48000,&ram2->memory,true);
    for(int i=0;i<160000;++i) {
        loop.SetTime(uint32_t(i/48)); reference::System::now=uint32_t(i/48);
        if(i==100) { loop.ToggleRecord(); originalLoop.ToggleRecord(); }
        if(i==24100 || i==48100) { loop.ToggleRecord(); originalLoop.ToggleRecord(); }
        if(i==55000) { loop.SetPitch(-.6f); originalLoop.SetPitch(-.6f); }
        if(i==90000) { loop.TogglePlaying(); originalLoop.TogglePlaying(); }
        if(i==96000) { loop.SetScrub(3); originalLoop.SetScrub(3); }
        if(i==99000) { loop.TogglePlaying(); originalLoop.TogglePlaying(); }
        if(i==110000) { loop.Reset(); originalLoop.Reset(); }
        loop.CheckRecordReady(); originalLoop.CheckRecordReady();
        loop.CheckReset(); originalLoop.CheckReset();
        float l=.2f*std::sin(float(i)*.045f),r=.1f*std::cos(float(i)*.079f);
        float ol=l,orr=r;
        loop.Process(&l,&r,1); originalLoop.Process(&ol,&orr,1);
        require(l==ol && r==orr,"Original tape transport/output comparison failed");
        require(loop.IsPlaying()==originalLoop.IsPlaying() && loop.IsRecording()==originalLoop.IsRecording(),"Transport state mismatch");
    }
    std::cout<<"Upstream comparison: 140000 sampler frames and 160000 tape frames matched exactly\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
