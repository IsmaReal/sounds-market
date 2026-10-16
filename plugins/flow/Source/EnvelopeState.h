#pragma once

#include "Envelope.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>

namespace flow
{

// Envolvente compartida entre la UI y el hilo de audio.
//
// La UI edita los nodos (protegidos por un CriticalSection) y cada cambio renderiza
// una tabla. El hilo de audio copia esa tabla con un try-lock al comienzo de cada
// bloque, así nunca se bloquea ni toca los nodos.
class EnvelopeState
{
public:
    static constexpr int tableSize = 4096;

    EnvelopeState() { setNodes ({ { 0.0, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }); }

    //==============================================================================
    // Hilo de mensajes
    std::vector<beatshaper::Node> getNodes() const
    {
        const juce::ScopedLock lock (nodesLock);
        return envelope.getNodes();
    }

    void setNodes (std::vector<beatshaper::Node> nodes)
    {
        std::array<float, tableSize + 1> table {};

        {
            const juce::ScopedLock lock (nodesLock);
            envelope.setNodes (std::move (nodes));
            envelope.renderTable (table.data(), tableSize);
        }

        {
            const juce::SpinLock::ScopedLockType lock (tableLock);
            sharedTable = table;
        }

        dirty.store (true);
        ++version;
    }

    int getVersion() const noexcept { return version.load(); }

    //==============================================================================
    // Hilo de audio
    void pullTable() noexcept
    {
        if (! dirty.load())
            return;

        const juce::SpinLock::ScopedTryLockType lock (tableLock);

        if (lock.isLocked())
        {
            audioTable = sharedTable;
            dirty.store (false);
        }
    }

    float lookup (double phase) const noexcept
    {
        const auto position = phase * tableSize;
        const auto index = juce::jlimit (0, tableSize - 1, (int) position);
        const auto frac = (float) (position - index);
        const auto a = audioTable[(size_t) index];
        return a + frac * (audioTable[(size_t) index + 1] - a);
    }

private:
    mutable juce::CriticalSection nodesLock;
    beatshaper::Envelope envelope;

    juce::SpinLock tableLock;
    std::array<float, tableSize + 1> sharedTable {};
    std::array<float, tableSize + 1> audioTable {};
    std::atomic<bool> dirty { true };
    std::atomic<int> version { 0 };
};

} // namespace flow
