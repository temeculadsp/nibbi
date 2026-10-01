#include "FactoryBank.h"
#include <stdexcept>

namespace nibbi {
namespace {
struct Factory {
    std::shared_ptr<const Catalog> bank;
    juce::String error;
};

juce::File archiveFile(const juce::String& name) {
    const auto binary=juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto parent=binary.getParentDirectory();
    for(const auto& candidate:{parent.getParentDirectory().getChildFile("Resources/resources").getChildFile(name),
                               parent.getChildFile("resources").getChildFile(name)})
        if(candidate.existsAsFile()) return candidate;
    return {};
}

std::vector<int16_t> pcm(juce::ZipFile& archive,const juce::String& name) {
    auto* entry=archive.getEntry(name);
    if(!entry) throw std::runtime_error("A factory sample is missing. Reinstall the complete NIBBI plug-in bundle.");
    std::unique_ptr<juce::InputStream> input(archive.createStreamForEntry(*entry));
    if(!input) throw std::runtime_error("Cannot read factory archive");
    juce::MemoryBlock bytes;
    input->readIntoMemoryBlock(bytes);
    const auto* data=static_cast<const uint8_t*>(bytes.getData());
    const size_t length=bytes.getSize();
    if(length<12 || std::memcmp(data,"RIFF",4) || std::memcmp(data+8,"WAVE",4))
        throw std::runtime_error("Invalid factory WAV");
    bool formatOK=false;
    for(size_t p=12;p+8<=length;) {
        const size_t size=juce::ByteOrder::littleEndianInt(data+p+4);
        if(size>length-p-8) throw std::runtime_error("Truncated factory WAV");
        const auto* payload=data+p+8;
        if(!std::memcmp(data+p,"fmt ",4)) {
            formatOK=size>=16 && juce::ByteOrder::littleEndianShort(payload)==1
                && juce::ByteOrder::littleEndianShort(payload+2)==2
                && juce::ByteOrder::littleEndianInt(payload+4)==48000
                && juce::ByteOrder::littleEndianShort(payload+14)==16;
        } else if(!std::memcmp(data+p,"data",4)) {
            if(!formatOK || size<4 || size%2) throw std::runtime_error("Unsupported factory WAV format");
            // Some supplied double-speed WAVs end with one unpaired 16-bit
            // channel sample. The firmware reader consumes complete stereo
            // frames, so discard only that incomplete final frame.
            const size_t completeBytes=size-size%4;
            std::vector<int16_t> result(completeBytes/2);
            // Preserve every complete 16-bit stereo frame exactly: no float
            // decode, normalization, resampling, or re-encoding.
            for(size_t i=0;i<result.size();++i)
                result[i]=static_cast<int16_t>(juce::ByteOrder::littleEndianShort(payload+2*i));
            return result;
        }
        p+=8+size+(size&1);
    }
    throw std::runtime_error("Factory WAV has no audio data");
}

Factory load() {
    Factory result;
    auto bank=std::make_shared<Catalog>();
    try {
        for(int mode=0;mode<2;++mode) for(int b=0;b<3;++b) {
            const auto archiveName=juce::String("TapeFactory-")+(mode?"cubbi-":"jammi-")
                +juce::String::charToString(juce::juce_wchar('a'+b))+".zip";
            const auto file=archiveFile(archiveName);
            if(!file.existsAsFile()) throw std::runtime_error((juce::String(mode?"HITKIT":"MELO")+" factory bank "+juce::String::charToString(juce::juce_wchar('A'+b))+" is missing. Reinstall the complete NIBBI plug-in bundle.").toStdString());
            juce::ZipFile archive(file);
            for(int slot=0;slot<14;++slot) {
            const auto stem=juce::String(mode?"cubbi_":"jammi_")
                +juce::String::charToString(juce::juce_wchar('a'+b))+juce::String(slot+1);
            auto sample=std::make_shared<Sample>();
            sample->pcm=pcm(archive,stem+".wav");
            sample->doublePcm=pcm(archive,stem+"_double.wav");
            sample->factoryId=mode*75+b*15+slot;
            const auto index=size_t(sample->factoryId);
            bank->samples[index]=sample;
            bank->names[index]=(juce::String(mode?"HITKIT ":"MELO ")
                +juce::String::charToString(juce::juce_wchar('A'+b))+juce::String(slot+1)).toStdString();
        }
        }
        // Slot 15 is the hardware's shared live buffer, not a factory recording.
        auto live=std::make_shared<Sample>(); live->live=true; live->pcm.resize(48000*3*2);
        float left=1.f,right=0.f;
        for(size_t n=0;n<live->frames();++n) {
            const float envelope=n<=24000?1.f:std::exp(-.00007195578f*float(n-24000));
            const float gain=std::min(1.f,float(n)/2000.f);
            left+=.5f*523.2511f*1.01f*(TWOPI_F/48000); right+=.5f*523.2511f*(TWOPI_F/48000);
            if(left>=TWOPI_F) left-=TWOPI_F; if(right>=TWOPI_F) right-=TWOPI_F;
            live->pcm[2*n]=int16_t(daisy::f2s16(gain*.4f*(2*std::abs(left/TWOPI_F-.5f)-1)*envelope));
            live->pcm[2*n+1]=int16_t(daisy::f2s16(gain*.4f*(2*std::abs(right/TWOPI_F-.5f)-1)*envelope));
        }
        bank->samples[14]=live; bank->names[14]="Live buffer (firmware startup tone)";
    } catch(const std::exception& e) {
        result.error=e.what();
        bank=std::make_shared<Catalog>(); // Never silently substitute another sound.
    }
    result.bank=std::move(bank);
    return result;
}
const Factory& sharedFactory() { static const Factory result=load(); return result; }
}
std::shared_ptr<const Catalog> factoryBank() { return sharedFactory().bank; }
juce::String factoryBankError() { return sharedFactory().error; }
}
