// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <wpi/SymbolExports.h>

#include "frc/geometry/Pose2d.h"
#include "frc/geometry/Rotation2d.h"
#include "units/angular_velocity.h"
#include "units/velocity.h"

namespace frc {
/**
 * Represents the speed of a robot chassis. Although this struct contains
 * similar members compared to a Twist2d, they do NOT represent the same thing.
 * Whereas a Twist2d represents a change in pose w.r.t to the robot frame of
 * reference, a ChassisSpeeds struct represents a robot's velocity.
 *
 * A strictly non-holonomic drivetrain, such as a differential drive, should
 * never have a dy component because it can never move sideways. Holonomic
 * drivetrains such as swerve and mecanum will often have all three components.
 */
struct WPILIB_DLLEXPORT ChassisSpeeds {
  /**
   * Velocity along the x-axis. (Fwd is +)
   */
  units::meters_per_second_t vx = 0_mps;

  /**
   * Velocity along the y-axis. (Left is +)
   */
  units::meters_per_second_t vy = 0_mps;

  /**
   * Represents the angular velocity of the robot frame. (CCW is +)
   */
  units::radians_per_second_t omega = 0_rad_per_s;

  /**
   * Disretizes a continuous-time chassis speed.
   *
   * This function converts a continous-time chassis speed into a discrete-time
   * one such that when the discrete-time chassis speed is applied for one
   * timestep, the robot moves as if the velocity components are independent
   * (i.e., the robot moves v_x * dt along the x-axis, v_y * dt along the
   * y-axis, and omega * dt around the z-axis).
   *
   * This is useful for compensating for translational skew when translating and
   * rotating a swerve drivetrain.
   *
   * @param vx Forward velocity.
   * @param vy Sideways velocity.
   * @param omega Angular velocity.
   * @param dt The duration of the timestep the speeds should be applied for.
   *
   * @return Discretized ChassisSpeeds.
   */
  static ChassisSpeeds Discretize(units::meters_per_second_t vx,
                                  units::meters_per_second_t vy,
                                  units::radians_per_second_t omega,
                                  units::second_t dt) {
    Pose2d desiredDeltaPose{vx * dt, vy * dt, omega * dt};
    auto twist = Pose2d{}.Log(desiredDeltaPose);
    return {twist.dx / dt, twist.dy / dt, twist.dtheta / dt};
  }

  /**
   * Disretizes a continuous-time chassis speed.
   *
   * This function converts a continous-time chassis speed into a discrete-time
   * one such that when the discrete-time chassis speed is applied for one
   * timestep, the robot moves as if the velocity components are independent
   * (i.e., the robot moves v_x * dt along the x-axis, v_y * dt along the
   * y-axis, and omega * dt around the z-axis).
   *
   * This is useful for compensating for translational skew when translating and
   * rotating a swerve drivetrain.
   *
   * @param continuousSpeeds The continuous speeds.
   * @param dt The duration of the timestep the speeds should be applied for.
   *
   * @return Discretized ChassisSpeeds.
   */
  static ChassisSpeeds Discretize(const ChassisSpeeds& continuousSpeeds,
                                  units::second_t dt) {
    return Discretize(continuousSpeeds.vx, continuousSpeeds.vy,
                      continuousSpeeds.omega, dt);
  }

  /**
   * Converts a user provided field-relative set of speeds into a robot-relative
   * ChassisSpeeds object.
   *
   * @param vx The component of speed in the x direction relative to the field.
   * Positive x is away from your alliance wall.
   * @param vy The component of speed in the y direction relative to the field.
   * Positive y is to your left when standing behind the alliance wall.
   * @param omega The angular rate of the robot.
   * @param robotAngle The angle of the robot as measured by a gyroscope. The
   * robot's angle is considered to be zero when it is facing directly away from
   * your alliance station wall. Remember that this should be CCW positive.
   *
   * @return ChassisSpeeds object representing the speeds in the robot's frame
   * of reference.
   */
  static ChassisSpeeds FromFieldRelativeSpeeds(
      units::meters_per_second_t vx, units::meters_per_second_t vy,
      units::radians_per_second_t omega, const Rotation2d& robotAngle) {
    return {vx * robotAngle.Cos() + vy * robotAngle.Sin(),
            -vx * robotAngle.Sin() + vy * robotAngle.Cos(), omega};
  }

  /**
   * Converts a user provided field-relative ChassisSpeeds object into a
   * robot-relative ChassisSpeeds object.
   *
   * @param fieldRelativeSpeeds The ChassisSpeeds object representing the speeds
   *    in the field frame of reference. Positive x is away from your alliance
   *    wall. Positive y is to your left when standing behind the alliance wall.
   * @param robotAngle The angle of the robot as measured by a gyroscope. The
   *    robot's angle is considered to be zero when it is facing directly away
   *    from your alliance station wall. Remember that this should be CCW
   *    positive.
   * @return ChassisSpeeds object representing the speeds in the robot's frame
   *    of reference.
   */
  static ChassisSpeeds FromFieldRelativeSpeeds(
      const ChassisSpeeds& fieldRelativeSpeeds, const Rotation2d& robotAngle) {
    return FromFieldRelativeSpeeds(fieldRelativeSpeeds.vx,
                                   fieldRelativeSpeeds.vy,
                                   fieldRelativeSpeeds.omega, robotAngle);
  }

  /**
   * Converts a user provided robot-relative set of speeds into a field-relative
   * ChassisSpeeds object.
   *
   * @param vx The component of speed in the x direction relative to the robot.
   * Positive x is towards the robot's front.
   * @param vy The component of speed in the y direction relative to the robot.
   * Positive y is towards the robot's left.
   * @param omega The angular rate of the robot.
   * @param robotAngle The angle of the robot as measured by a gyroscope. The
   * robot's angle is considered to be zero when it is facing directly away from
   * your alliance station wall. Remember that this should be CCW positive.
   *
   * @return ChassisSpeeds object representing the speeds in the field's frame
   * of reference.
   */
  static ChassisSpeeds FromRobotRelativeSpeeds(
      units::meters_per_second_t vx, units::meters_per_second_t vy,
      units::radians_per_second_t omega, const Rotation2d& robotAngle) {
    return {-vx * -robotAngle.Cos() - vy * -robotAngle.Sin(),
            vx * -robotAngle.Sin() - vy * -robotAngle.Cos(), omega};
  }

  /**
   * Converts a user provided robot-relative ChassisSpeeds object into a
   * field-relative ChassisSpeeds object.
   *
   * @param robotRelativeSpeeds The ChassisSpeeds object representing the speeds
   *    in the robot frame of reference. Positive x is the towards robot's
   * front. Positive y is towards the robot's left.
   * @param robotAngle The angle of the robot as measured by a gyroscope. The
   *    robot's angle is considered to be zero when it is facing directly away
   *    from your alliance station wall. Remember that this should be CCW
   *    positive.
   * @return ChassisSpeeds object representing the speeds in the field's frame
   *    of reference.
   */
  static ChassisSpeeds FromRobotRelativeSpeeds(
      const ChassisSpeeds& robotRelativeSpeeds, const Rotation2d& robotAngle) {
    return FromRobotRelativeSpeeds(robotRelativeSpeeds.vx,
                                   robotRelativeSpeeds.vy,
                                   robotRelativeSpeeds.omega, robotAngle);
  }

  /**
   * Adds two ChassisSpeeds and returns the sum.
   *
   * <p>For example, ChassisSpeeds{1.0, 0.5, 1.5} + ChassisSpeeds{2.0, 1.5, 0.5}
   * = ChassisSpeeds{3.0, 2.0, 2.0}
   *
   * @param other The ChassisSpeeds to add.
   *
   * @return The sum of the ChassisSpeeds.
   */
  constexpr ChassisSpeeds operator+(const ChassisSpeeds& other) const {
    return {vx + other.vx, vy + other.vy, omega + other.omega};
  }

  /**
   * Subtracts the other ChassisSpeeds from the current ChassisSpeeds and
   * returns the difference.
   *
   * <p>For example, ChassisSpeeds{5.0, 4.0, 2.0} - ChassisSpeeds{1.0, 2.0, 1.0}
   * = ChassisSpeeds{4.0, 2.0, 1.0}
   *
   * @param other The ChassisSpeeds to subtract.
   *
   * @return The difference between the two ChassisSpeeds.
   */
  constexpr ChassisSpeeds operator-(const ChassisSpeeds& other) const {
    return *this + -other;
  }

  /**
   * Returns the inverse of the current ChassisSpeeds.
   * This is equivalent to negating all components of the ChassisSpeeds.
   *
   * @return The inverse of the current ChassisSpeeds.
   */
  constexpr ChassisSpeeds operator-() const { return {-vx, -vy, -omega}; }

  /**
   * Multiplies the ChassisSpeeds by a scalar and returns the new ChassisSpeeds.
   *
   * <p>For example, ChassisSpeeds{2.0, 2.5, 1.0} * 2
   * = ChassisSpeeds{4.0, 5.0, 1.0}
   *
   * @param scalar The scalar to multiply by.
   *
   * @return The scaled ChassisSpeeds.
   */
  constexpr ChassisSpeeds operator*(double scalar) const {
    return {scalar * vx, scalar * vy, scalar * omega};
  }

  /**
   * Divides the ChassisSpeeds by a scalar and returns the new ChassisSpeeds.
   *
   * <p>For example, ChassisSpeeds{2.0, 2.5, 1.0} / 2
   * = ChassisSpeeds{1.0, 1.25, 0.5}
   *
   * @param scalar The scalar to divide by.
   *
   * @return The scaled ChassisSpeeds.
   */
  constexpr ChassisSpeeds operator/(double scalar) const {
    return operator*(1.0 / scalar);
  }
};
}  // namespace frc
