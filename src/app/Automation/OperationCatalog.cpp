#include "OperationCatalog.h"

#include <algorithm>

namespace Automation {

    AutomationResult<AutomationUnit> OperationCatalog::add(OperationDescriptor descriptor) {
        if (descriptor.id.trimmed().isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("id"),
                                                    QStringLiteral("Operation ID is required"));
        }
        if (find(descriptor.id)) {
            return AutomationError::invalidArgument(
                QStringLiteral("id"), QStringLiteral("Operation ID is already registered"));
        }

        m_entries.append(std::move(descriptor));
        std::sort(m_entries.begin(), m_entries.end(),
                  [](const auto &left, const auto &right) { return left.id < right.id; });
        return AutomationUnit{};
    }

    const QList<OperationDescriptor> &OperationCatalog::entries() const {
        return m_entries;
    }

    const OperationDescriptor *OperationCatalog::find(const OperationId &id) const {
        const auto it = std::lower_bound(
            m_entries.cbegin(), m_entries.cend(), id,
            [](const OperationDescriptor &descriptor, const OperationId &requested) {
                return descriptor.id < requested;
            });
        if (it == m_entries.cend() || it->id != id)
            return nullptr;
        return &*it;
    }

    QStringList OperationCatalog::operationIds() const {
        QStringList result;
        result.reserve(m_entries.size());
        for (const auto &entry : m_entries)
            result.append(entry.id);
        return result;
    }

} // namespace Automation
