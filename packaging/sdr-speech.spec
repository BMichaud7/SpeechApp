%global debug_package %{nil}
Name:       sdr-speech
Version:    %{pkg_version}
Release:    1%{?dist}
Summary:    OpenRFStack whisper.cpp voice transcription for P25 audio
License:    Proprietary
URL:        https://github.com/OpenRFStack/SpeechApp
BuildArch:  x86_64
AutoReqProv: no
Requires:   qpid-proton-cpp tinyxml2 spdlog fmt

%description
SpeechApp transcribes P25 voice channel audio using whisper.cpp. Consumes
voice frames from the AMQP voice.frames queue published by DemodApp and
publishes text transcripts. Supports GPU-accelerated inference (0.12x
realtime factor) and CPU fallback.

%prep
%build
%install

%files
/usr/bin/sdr_speech

%changelog
* Thu Jan 01 2026 OpenRFStack CI <noreply@github.com> - %{pkg_version}-1
- Automated build from main/1.0
