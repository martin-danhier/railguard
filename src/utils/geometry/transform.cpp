#include "railguard/utils/geometry/transform.h"

namespace rg
{


    glm::mat4 Transform::view_matrix() const
    {
        auto view = glm::mat4(1.0f);
        view      = glm::translate(view, position);
        view      = glm::mat4_cast(rotation) * view;
        view      = glm::scale(view, scale);
        return view;
    }


} // namespace rg
