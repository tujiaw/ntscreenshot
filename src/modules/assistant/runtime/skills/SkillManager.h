#pragma once

#include <QString>

namespace LlmSkills {

class SkillManager {
public:
    static QString buildPrompt(const QString &message);
    static QString resolveSkillRoot();
};

} // namespace LlmSkills
