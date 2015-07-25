#include "stdafx.h"
#include "Formats.h"

#include "../shellres/resource.h"

namespace MediaFormats
{
    void GetFormats(const Format** formats, size_t* count)
    {
        static const Format Formats[] = {
            { Type::Video, IDI_SH_AVI,     IDS_SH_AVI,    _T("avi"),    _T(".avi") },
            { Type::Video, IDI_SH_MPEG,    IDS_SH_MPG,    _T("mpeg"),   _T(".mpeg .mpg .mpe .m1v .m2v .mpv2 .mp2v .pva .evo .m2p") },
            { Type::Video, IDI_SH_TS,      IDS_SH_TS,     _T("ts"),     _T(".ts .tp .trp .m2t .m2ts .mts .rec") },
            { Type::Video, IDI_SH_VOB,     IDS_SH_VOB,    _T("vob"),    _T(".vob") },
            { Type::Video, IDI_SH_IFO,     IDS_SH_IFO,    _T("ifo"),    _T(".ifo") },
            { Type::Video, IDI_SH_MKV,     IDS_SH_MKV,    _T("mkv"),    _T(".mkv") },
            { Type::Video, IDI_SH_WEBM,    IDS_SH_WEBM,   _T("webm")    _T(".webm") },
            { Type::Video, IDI_SH_MP4,     IDS_SH_MP4,    _T("mp4"),    _T(".mp4 .m4v .mp4v .mpv4 .hdmov") },
            { Type::Video, IDI_SH_MOV,     IDS_SH_MOV,    _T("mov"),    _T(".mov") },
            { Type::Video, IDI_SH_MP4,     IDS_SH_3GP,    _T("3gp"),    _T(".3gp .3gpp .3ga") },
            { Type::Video, IDI_SH_MOV,     IDS_SH_3G2,    _T("3g2"),    _T(".3g2 .3gp2") },
            { Type::Video, IDI_SH_FLV,     IDS_SH_FLV,    _T("flv"),    _T(".flv .f4v") },
            { Type::Video, IDI_SH_OGM,     IDS_SH_OGM,    _T("ogm"),    _T(".ogm .ogv") },
            { Type::Video, IDI_SH_RM,      IDS_SH_RM,     _T("rm"),     _T(".rm .rmvb") },
            { Type::Video, IDI_SH_RT,      IDS_SH_RT,     _T("rt"),     _T(".rt .ram .rpm .rmm .rp .smi .smil") },
            { Type::Video, IDI_SH_WMV,     IDS_SH_WMV,    _T("wmv"),    _T(".wmv .wmp .wm .asf") },
            { Type::Video, IDI_SH_BINK,    IDS_SH_BINK,   _T("bink"),   _T(".bik") },
            { Type::Video, IDI_SH_SMK,     IDS_SH_SMK,    _T("smk"),    _T(".smk") },
            { Type::Video, IDI_SH_FLIC,    IDS_SH_FLIC,   _T("flic"),   _T(".flic .fli .flc") },
            { Type::Video, IDI_SH_DSM,     IDS_SH_DSM,    _T("dsm"),    _T(".dsm .dsv .dsa .dss") },
            { Type::Video, IDI_SH_IVF,     IDS_SH_IVF,    _T("ivf"),    _T(".ivf") },
            { Type::Video, IDI_SH_DEFAULT, IDS_SH_MXF,    _T("mxf"),    _T(".mxf") },
            { Type::Video, IDI_SH_DEFAULT, IDS_SH_VIDEOF, _T("videof"), _T(".divx .amv") },

            { Type::Audio, IDI_SH_AC3,     IDS_SH_AC3,    _T("ac3"),    _T(".ac3") },
            { Type::Audio, IDI_SH_DTS,     IDS_SH_DTS,    _T("dts"),    _T(".dts .dtshd .dtsma") },
            { Type::Audio, IDI_SH_AIFF,    IDS_SH_AIFF,   _T("aiff"),   _T(".aiff .aif .aifc") },
            { Type::Audio, IDI_SH_ALAC,    IDS_SH_ALAC,   _T("alac"),   _T(".alac") },
            { Type::Audio, IDI_SH_AMR,     IDS_SH_AMR,    _T("amr"),    _T(".amr") },
            { Type::Audio, IDI_SH_APE,     IDS_SH_APE,    _T("ape"),    _T(".ape .apl") },
            { Type::Audio, IDI_SH_AU,      IDS_SH_AU,     _T("au"),     _T(".au .snd") },
            { Type::Audio, IDI_SH_CDA,     IDS_SH_CDA,    _T("cda"),    _T(".cda") },
            { Type::Audio, IDI_SH_FLAC,    IDS_SH_FLAC,   _T("flac"),   _T(".flac") },
            { Type::Audio, IDI_SH_AAC,     IDS_SH_M4A,    _T("m4a"),    _T(".m4a .m4b .m4r .aac") },
            { Type::Audio, IDI_SH_MIDI,    IDS_SH_MIDI,   _T("midi"),   _T(".mid .midi .rmi") },
            { Type::Audio, IDI_SH_MKA,     IDS_SH_MKA,    _T("mka"),    _T(".mka") },
            { Type::Audio, IDI_SH_MP3,     IDS_SH_MP3,    _T("mp3"),    _T(".mp3") },
            { Type::Audio, IDI_SH_MPA,     IDS_SH_MPA,    _T("mpa"),    _T(".mpa .mp2 .m1a .m2a") },
            { Type::Audio, IDI_SH_MPC,     IDS_SH_MPC,    _T("mpc"),    _T(".mpc") },
            { Type::Audio, IDI_SH_OFR,     IDS_SH_OFR,    _T("ofr"),    _T(".ofr .ofs") },
            { Type::Audio, IDI_SH_OGG,     IDS_SH_OGG,    _T("ogg"),    _T(".ogg .oga") },
            { Type::Audio, IDI_SH_DEFAULT, IDS_SH_OPUS,   _T("opus"),   _T(".opus") },
            { Type::Audio, IDI_SH_RA,      IDS_SH_RA,     _T("ra"),     _T(".ra") },
            { Type::Audio, IDI_SH_DEFAULT, IDS_SH_TAK,    _T("tak"),    _T(".tak") },
            { Type::Audio, IDI_SH_TTA,     IDS_SH_TTA,    _T("tta"),    _T(".tta") },
            { Type::Audio, IDI_SH_WAV,     IDS_SH_WAV,    _T("wav"),    _T(".wav") },
            { Type::Audio, IDI_SH_WMA,     IDS_SH_WMA,    _T("wma"),    _T(".wma") },
            { Type::Audio, IDI_SH_WV,      IDS_SH_WV,     _T("wv"),     _T(".wv") },
            { Type::Audio, IDI_SH_DEFAULT, IDS_SH_AUDIOF, _T("audiof"), _T(".aob .mlp") },

            { Type::PlaylistAudio, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("pls"),   _T(".pls") },
            { Type::PlaylistAudio, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("asx"),   _T(".asx") },
            { Type::PlaylistAudio, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("wax"),   _T(".wax") },
            { Type::PlaylistAudio, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("m3u"),   _T(".m3u") },

            { Type::PlaylistVideo, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("mpcpl"), _T(".mpcpl") },
            { Type::PlaylistVideo, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("wvx"),   _T(".wvx .wmx") },
            { Type::PlaylistVideo, IDI_SH_PLAYLIST, IDS_SH_PLS,  _T("m3u8"),  _T(".m3u8") },
            { Type::PlaylistVideo, IDI_SH_PLAYLIST, IDS_SH_BDMV, _T("bdmv"),  _T(".bdmv .mpls") },

            { Type::Misc, IDI_SH_SWF,     IDS_SH_SWF, _T("swf"), _T(".swf") },
            //Type::Misc, IDI_SH_GENERIC, IDS_SH_RAR, _T("rar"), _T(".rar") },
        };

        ASSERT(formats);
        ASSERT(count);
        *formats = Formats;
        *count = _countof(Formats);
    }

    void GetProtocols(const Protocol** protocols, size_t* count)
    {
        static const Protocol Protocols[] = {
            { _T("url.rtp"),  IDI_SH_DEFAULT, IDS_SH_RTP, _T("rtp") },
            { _T("url.mms"),  IDI_SH_DEFAULT, IDS_SH_MMS, _T("mms mmsh") },
            { _T("url.rtmp"), IDI_SH_DEFAULT, IDS_SH_RTMP, _T("rtmp rtmpt") },
            { _T("url.rtsp"), IDI_SH_DEFAULT, IDS_SH_RTSP, _T("rtsp rtspu rtspm rtspt rtsph") },
        };

        ASSERT(protocols);
        ASSERT(count);
        *protocols = Protocols;
        *count = _countof(Protocols);
    }
}
