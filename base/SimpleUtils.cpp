#include "SimpleUtils.h"

VkTransformMatrixKHR SimpleUtils::glmToVkMatrix(const glm::mat4& glmMat4)
{
    VkTransformMatrixKHR resultMat;
	//glm::mat4 column-major, VkTransformMatrixKHR row-major
    std::memcpy(&resultMat, &glmMat4, sizeof(resultMat));

    resultMat.matrix[0][3] = glmMat4[3][0]; // Translate x
    resultMat.matrix[1][3] = glmMat4[3][1]; // Translate y
    resultMat.matrix[2][3] = glmMat4[3][2]; // Translate z
    return resultMat;
}

glm::mat4 SimpleUtils::VkMatrixToGlm(const VkTransformMatrixKHR& vkMat)
{
    glm::mat4 resultMat;

    // VkTransformMatrixKHR is row-major, glm::mat4 is column-major
    glm::mat4 tempMat;
    std::memcpy(&tempMat, &vkMat, sizeof(vkMat));

    resultMat = glm::transpose(tempMat);

    resultMat[3][0] = vkMat.matrix[0][3]; // Translate x
    resultMat[3][1] = vkMat.matrix[1][3]; // Translate y
    resultMat[3][2] = vkMat.matrix[2][3]; // Translate z

    return resultMat;
}

glm::vec3 SimpleUtils::Scale(const glm::mat4& transform)
{
    return glm::vec3(transform[0][0], transform[1][1], transform[2][2]);
}

glm::vec3 SimpleUtils::Right(const glm::mat4& transform)
{
    return glm::normalize(glm::vec3(transform[0]));
}

glm::vec3 SimpleUtils::Up(const glm::mat4& transform)
{
    return glm::normalize(glm::vec3(transform[1]));
}

glm::vec3 SimpleUtils::Look(const glm::mat4& transform)
{
    return glm::normalize(glm::vec3(transform[2]));
}

glm::vec3 SimpleUtils::Position(const glm::mat4& transform)
{
    return glm::vec3(transform[3].x, transform[3].y, transform[3].z);
}

glm::mat4 SimpleUtils::ScaleMatAccumulated(const glm::mat4& transform, glm::vec3 addingScale)
{
    glm::mat4 curScale;
    curScale[0].x = transform[0].x;
    curScale[1].x = transform[1].y;
    curScale[2].x = transform[2].z;

    return glm::scale(curScale, addingScale);
}

glm::mat4 SimpleUtils::RotationMatAccumulated(const glm::mat4& transform, float angle, glm::vec3 axis)
{
    glm::mat4 curRotate = glm::mat4(
        glm::vec4(SimpleUtils::Right(transform), 0.f),
        glm::vec4(SimpleUtils::Up(transform), 0.f),
        glm::vec4(SimpleUtils::Look(transform), 0.f),
        glm::vec4(0.f, 0.f, 0.f, 1.f)
    );

    return glm::rotate(curRotate, angle, axis);
}

glm::mat4 SimpleUtils::RotationMatAccumulated(const glm::mat4& transform, const glm::mat4& rotate)
{
    glm::mat4 curRotate = glm::mat4(
        glm::vec4(SimpleUtils::Right(transform), 0.f),
        glm::vec4(SimpleUtils::Up(transform), 0.f),
        glm::vec4(SimpleUtils::Look(transform), 0.f),
        glm::vec4(0.f, 0.f, 0.f, 1.f)
    );
    return curRotate * rotate;
}

glm::mat4 SimpleUtils::TranslationMatAccumulated(const glm::mat4& transform, glm::vec3 addingPos)
{
    glm::mat4 curTranslate;
    memcpy(&curTranslate[3], &transform[3], sizeof(glm::vec4)); // copy translate

    return glm::translate(curTranslate, addingPos);
}

glm::mat4 SimpleUtils::GetRotation(const glm::mat4& transform)
{
    return glm::mat4(
        glm::vec4(SimpleUtils::Right(transform), 0.f),
        glm::vec4(SimpleUtils::Up(transform), 0.f),
        glm::vec4(SimpleUtils::Look(transform), 0.f),
        glm::vec4(0.f, 0.f, 0.f, 1.f)
    );
}

glm::mat4 SimpleUtils::GetTranslation(const glm::mat4& transform)
{
    glm::mat4 curTranslate;
    memcpy(&curTranslate[3], &transform[3], sizeof(glm::vec4)); // copy translate

    return curTranslate;
}

void SimpleUtils::PrintCamPos(const glm::mat4& view)
{
    using std::cout, std::endl;
	const glm::vec4& camPos = view[3];
    cout << "Cam Pos\tx: " << camPos.x << "\ty: " << camPos.y << "\tz: " << camPos.z << endl;
}
