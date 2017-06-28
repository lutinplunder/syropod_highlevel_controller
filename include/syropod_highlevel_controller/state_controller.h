#ifndef SYROPOD_HIGHLEVEL_CONTROLLER_STATE_CONTROLLER_H
#define SYROPOD_HIGHLEVEL_CONTROLLER_STATE_CONTROLLER_H
/*******************************************************************************************************************//**
 *  @file    state_controller.h
 *  @brief   Top level controller that handles state of Syropod.
 *
 *  @author  Fletcher Talbot (fletcher.talbot@csiro.au)
 *  @date    June 2017
 *  @version 0.5.0
 *
 *  CSIRO Autonomous Systems Laboratory
 *  Queensland Centre for Advanced Technologies
 *  PO Box 883, Kenmore, QLD 4069, Australia
 *
 *  (c) Copyright CSIRO 2017
 *
 *  All rights reserved, no part of this program may be used
 *  without explicit permission of CSIRO
 *
***********************************************************************************************************************/
 
#include "standard_includes.h"
#include "parameters_and_states.h"

#include "syropod_highlevel_controller/DynamicConfig.h"

#include "walk_controller.h"
#include "pose_controller.h"
#include "model.h"

#include "debug_output.h"
#include "impedance_controller.h"

#define MAX_MANUAL_LEGS 2 // Maximum number of legs able to be manually manipulated simultaneously
#define PACK_TIME 2.0 // Joint transition time during pack/unpack sequences (seconds @ step frequency == 1.0)

/*******************************************************************************************************************//**
 * This class creates and initialises all ros publishers/subscriptions; sub-controllers: Walk Controller,
 * Pose Controller and Impedance Controller; and the parameter handling struct. It handles all the ros publishing and
 * subscription callbacks and communicates this data among the robot model class and the sub-controller classes.
 * The primary function of this class is to feed desired body velocity inputs to the walk controller and receive 
 * generated leg trajectories. These trajectories are then fed to the pose controller where any required body posing is
 * applied and a desired tip position for each leg is generated. Finally, this class calls the robot model to apply
 * inverse kinematics for each leg's desired tip position and generates desired joint positions which are then 
 * published. This class also manages transitions between the system state and the robot state (defined in 
 * parameters_and_states.h) and manages gait change events, parameter adjustment events and leg state transitions.
***********************************************************************************************************************/
class StateController
{
public:
  /**
   * StateController class constructor. Initialises parameters, creates robot model object, sets up ros topic
   * subscriptions and advertisments.
   * @param[in] n The ros node handle, used to subscribe/publish topics and assign callbacks
   * @todo Refactor tip force subscribers into single callback to topic /tip_forces
   * @todo Remove ASC publisher object
   */
  StateController(const ros::NodeHandle& n);
  
  /** StateController object destructor */
  ~StateController(void);

  /** Accessor for parameter member */
  inline const Parameters& getParameters(void) { return params_; };
  
  /** Accessor for system state member */
  inline SystemState getSystemState(void) { return system_state_; };
  
  /** Returns true if all joint objects in model have been initialised with a current position */
  inline bool jointPositionsInitialised(void) { return joint_positions_initialised_; };
  
  /** Initialises the model by calling the model object function initLegs() */
  inline void initModel(const bool& use_default_joint_positions = false)
  { 
    model_->initLegs(use_default_joint_positions); 
  };

  /**
   * StateController initialiser function. Initialises member variables: robot state, gait selection and initalisation
   * flag and creates sub controller objects: WalkController, PoseController and ImpedanceController.
   */
  void init(void);
  
  /** 
   * Acquires parameter values from the ros param server and initialises parameter objects. Also sets up dynamic 
   * reconfigure server.
   */
  void initParameters(void);
  
  /**
   * Acquires gait selection defined parameter values from the ros param server and initialises parameter objects.
   * @param[in] gait_selection The desired gait used to acquire associated parameters off the parameter server
   */
  void initGaitParameters(const GaitDesignation& gait_selection);
  
  /** Acquires auto pose parameter values from the ros param server and initialises parameter objects. */
	void initAutoPoseParameters(void);

  /** 
   * The main loop of the state controller (called from the main ros loop).
   * Coordinates with other controllers to update based on current robot state, also calls for state transitions. 
   */
  void loop(void);
  /**
   * Handles transitions of robot state and moves the robot as required for the new state.
   * The transition from one state to another may require several iterations through this function before ending.
   */
  void transitionRobotState(void);
  /**
   * Loops whilst robot is in RUNNING state.
   * Coordinates changes of gait, parameter adjustments, leg state toggling and the application of cruise control.
   * Updates the walk/pose controllers tip positions and applies inverse kinematics to the leg objects.
   */
  void runningState(void);
  /**
   * Handles parameter adjustment. Forces robot velocity input to zero until it is in a STOPPED walk state and then 
   * reinitialises the walk/pose/impedance controllers with the new parameter value to be applied. The pose controller
   * is then called to step to new stance if required.
   */
  void adjustParameter(void);
  /**
   * Handles a gait change event. Forces robot velocity input to zero until it is in a STOPPED walk state and then 
   * updates gait parameters based on the new gait selection and reinitialises the walk controller with the new 
   * parameters. If required the pose controller is reinitialised with new 'auto posing' parameters.
   */
  void changeGait(void);
  /**
   * Handles a leg toggle event. Forces robot velocity input to zero until it is in a STOPPED walk state and then 
   * calculates a new default pose based on estimated loading patterns. The leg that is changing state is assigned the 
   * new state and any posing for the new state is executed via the pose controller.
   */
  void legStateToggle(void);
  /**
   * Iterates through leg objects and either collates joint state information for combined publishing and/or publishes
   * the desired joint position on the leg member publisher object.
   */
  void publishDesiredJointState(void);

  // Debugging functions
  /**
   * Iterates through leg objects and collates state information for publishing on custom leg state message topic
   * @todo Remove ASC state messages in line with requested hardware changes to use legState message variable/s
   */
  void publishLegState(void);
  
  /** Publishes body velocity for debugging */
  void publishBodyVelocity(void);
  
  /** Publishes current pose (roll, pitch, yaw, x, y, z) for debugging */
  void publishPose(void);
  
  /** Publishes current rotation as per the IMU data object (roll, pitch, yaw, x, y, z) for debugging */
  void publishIMUData(void);
  
  /** Publishes imu pose rotation absement, position and velocity errors used in the PID controller, for debugging */
  void publishRotationPoseError(void);
  
  /**
   * Sets up velocities for and calls debug output object to publish various debugging visualations via rviz
   * @param[in] static_display Flag which determines if the vizualisation is kept statically in place at the origin
   * @todo Implement calculation of actual body velocity
   */
  void RVIZDebugging(const bool& static_display = false);

  /**
   * Callback handling the desired system state. Sends message to user interface when system enters OPERATIONAL state.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/system_state"
   * @see parameters_and_states.h
   */
  void systemStateCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the desired robot state.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/robot_state"
   * @see parameters_and_states.h
   */
  void robotStateCallback(const std_msgs::Int8& input);
  
  /**
   * Callback for the input body velocity
   * @param[in] input The Twist geometry message provided by the subscribed ros topic "syropod_remote/desired_velocity"
   */
  void bodyVelocityInputCallback(const geometry_msgs::Twist& input);
  
  /**
   * Callback for the input body pose velocity (x/y/z linear translation and roll/pitch/yaw angular rotation velocities)
   * @param[in] input The Twist geometry message provided by the subscribed ros topic "syropod_remote/desired_pose"
   */
  void bodyPoseInputCallback(const geometry_msgs::Twist& input);
  
  /**
   * Callback handling the desired posing mode and sending state messages to user interface.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/posing_mode"
   * @see parameters_and_states.h
   */
  void posingModeCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling desired pose reset mode
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/pose_reset_mode"
   */
  void poseResetCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the desired gait selection.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/gait_selection"
   * @see parameters_and_states.h
   */
  void gaitSelectionCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the cruise control mode and sending state messages to user interface. Determines cruise velocity
   * from either parameters or current velocitiy inputs.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/cruise_control_mode"
   * @see parameters_and_states.h
   */
  void cruiseControlCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the auto navigation mode and sending state messages to user interface.
   * @param[in] input The Int8 standard message provided by the subscribed topic "syropod_remote/auto_navigation_mode"
   * @see parameters_and_states.h
   */
  void autoNavigationCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the selection of the leg as the primary leg for manual manipulation.
   * @param[in] input The Int8 standard message provided by the subscribed topic "syropod_remote/primary_leg_selection"
   * @see parameters_and_states.h
   */
  void primaryLegSelectionCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the selection of the leg as the secondary leg for manual manipulation.
   * @param[in] input The Int8 standard message provided by the topic "syropod_remote/secondary_leg_selection"
   * @see parameters_and_states.h
   */
  void secondaryLegSelectionCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the toggling the state of the primary selected leg.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/primary_leg_state"
   * @see parameters_and_states.h
   */
  void primaryLegStateCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the toggling the state of the secondary selected leg.
   * @param[in] input The Int8 standard message provided by the subscribed ros topic "syropod_remote/secondary_leg_state"
   * @see parameters_and_states.h
   */
  void secondaryLegStateCallback(const std_msgs::Int8& input);
  
  /**
   * Callback for the input manual tip velocity (in cartesian space) for the primary selected leg
   * @param[in] input The Point geometry message provided by the subscribed topic "syropod_remote/primary_tip_velocity"
   */
  void primaryTipVelocityInputCallback(const geometry_msgs::Point& input);
  
  /**
   * Callback for the input manual tip velocity (in cartesian space) for the secondary selected leg
   * @param[in] input The Point geometry message provided by the topic "syropod_remote/secondary_tip_velocity"
   */
  void secondaryTipVelocityInputCallback(const geometry_msgs::Point& input);
  
  /**
   * Callback handling the desired parameter selection and sending state messages to user interface.
   * @param[in] input The Int8 standard message provided by the subscribed topic "syropod_remote/parameter_selection"
   * @see parameters_and_states.h
   */
  void parameterSelectionCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling the desired selected parameter adjustment. Sets a new value for the selected parameter by adding
   * or subtracting the (parameter defined) adjustment value according to the input direction and clamped with limits.
   * @param[in] input The Int8 standard message provided by the subscribed topic "syropod_remote/parameter_adjustment"
   * @see parameters_and_states.h
   */
  void parameterAdjustCallback(const std_msgs::Int8& input);
  
  /**
   * Callback handling new configurations from a dynamic reconfigure client and assigning new values for adjustment.
   * @param[in] config The new configuration sent from a dynamic reconfigure client (eg: rqt_reconfigure)
   * @param[in] level Unused
   * @see config/dynamic_parameter.cfg
   */
	void dynamicParameterCallback(syropod_highlevel_controller::DynamicConfig& config, const uint32_t& level);

  /**
   * Callback handling the transformation of IMU data from imu frame to base link frame
   * @param[in] data The Imu sensor message provided by the subscribed ros topic "/imu/data"
   * @todo Use tf2 for transformation between frames rather than parameters
   */
  void imuCallback(const sensor_msgs::Imu& data);
  
  /**
   * Callback which handles acquisition of joint states from motor drivers. Attempts to populate joint objects with 
   * available current position/velocity/effort and flags if all joint objects have received an initial current position
   * @param[in] joint_states The JointState sensor message provided by the subscribed ros topic "/joint_states"
   */
  void jointStatesCallback(const sensor_msgs::JointState& joint_states);
  
  /**
   * Callback handling and normalising the MAX specific pressure sensor data
   * @param[in] raw_tip_forces The JointState sensor message provided by the subscribed ros topic "/motor_encoders" 
   * which contains the pressure sensor data.
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallback(const sensor_msgs::JointState& raw_tip_forces);
  
  /**
   * Callback handling and normalising the Flexipod (AR leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/AR_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackAR(const std_msgs::UInt16& raw_tip_force);
  
  /**
   * Callback handling and normalising the Flexipod (BR leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/BR_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackBR(const std_msgs::UInt16& raw_tip_force);
  
  /**
   * Callback handling and normalising the Flexipod (CR leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/CR_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackCR(const std_msgs::UInt16& raw_tip_force);
  
  /**
   * Callback handling and normalising the Flexipod (CL leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/CL_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackCL(const std_msgs::UInt16& raw_tip_force);
  
  /**
   * Callback handling and normalising the Flexipod (BL leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/BL_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackBL(const std_msgs::UInt16& raw_tip_force);
  
  /**
   * Callback handling and normalising the Flexipod (AL leg) specific pressure sensor data
   * @param[in] raw_tip_force  The UInt16 standard message provided by the subscribed ros topic "/AL_prs"
   * @todo Refactor this callback and all other tip force callbacks into a single callback to the topic "/tip_forces"
   * @todo Parameterise the normalisation variables
   */
  void tipForceCallbackAL(const std_msgs::UInt16& raw_tip_force);
  
private:
  ros::NodeHandle n_;                                 ///! Ros node handle
  
  ros::Subscriber system_state_subscriber_;           ///! Subscriber for topic "syropod_remote/system_state"
  ros::Subscriber robot_state_subscriber_;            ///! Subscriber for topic "syropod_remote/robot_state"  
  ros::Subscriber desired_velocity_subscriber_;       ///! Subscriber for topic "syropod_remote/desired_velocity"
  ros::Subscriber desired_pose_subscriber_;           ///! Subscriber for topic "syropod_remote/desired_pose"
  ros::Subscriber posing_mode_subscriber_;            ///! Subscriber for topic "syropod_remote/posing_mode"
  ros::Subscriber pose_reset_mode_subscriber_;        ///! Subscriber for topic "syropod_remote/pose_reset_mode"
  ros::Subscriber gait_selection_subscriber_;         ///! Subscriber for topic "syropod_remote/gait_selection"
  ros::Subscriber cruise_control_mode_subscriber_;    ///! Subscriber for topic "syropod_remote/cruise_control_mode"
  ros::Subscriber auto_navigation_mode_subscriber_;   ///! Subscriber for topic "syropod_remote/auto_navigation_mode"
  ros::Subscriber primary_leg_selection_subscriber_;  ///! Subscriber for topic "syropod_remote/primary_leg_selection"
  ros::Subscriber primary_leg_state_subscriber_;      ///! Subscriber for topic "syropod_remote/primary_leg_state"
  ros::Subscriber primary_tip_velocity_subscriber_;   ///! Subscriber for topic "syropod_remote/primary_tip_velocity"
  ros::Subscriber secondary_leg_selection_subscriber_;///! Subscriber for topic "syropod_remote/secondary_leg_selection"
  ros::Subscriber secondary_leg_state_subscriber_;    ///! Subscriber for topic "syropod_remote/secondary_leg_state"
  ros::Subscriber secondary_tip_velocity_subscriber_; ///! Subscriber for topic "syropod_remote/secondary_tip_velocity"
  ros::Subscriber parameter_selection_subscriber_;    ///! Subscriber for topic "syropod_remote/parameter_selection"
  ros::Subscriber parameter_adjustment_subscriber_;   ///! Subscriber for topic "syropod_remote/parameter_adjustment"
  ros::Subscriber imu_data_subscriber_;               ///! Subscriber for topic "/imu/data"
  ros::Subscriber joint_state_subscriber_;            ///! Subscriber for topic "/joint_states"
  ros::Subscriber tip_force_subscriber_;              ///! Subscriber for topic "/motor_encoders"
  ros::Subscriber tip_force_subscriber_AR_;           ///! Subscriber for topic "/AR_prs"
  ros::Subscriber tip_force_subscriber_BR_;           ///! Subscriber for topic "/BR_prs"
  ros::Subscriber tip_force_subscriber_CR_;           ///! Subscriber for topic "/CR_prs"
  ros::Subscriber tip_force_subscriber_CL_;           ///! Subscriber for topic "/CL_prs"
  ros::Subscriber tip_force_subscriber_BL_;           ///! Subscriber for topic "/BL_prs"
  ros::Subscriber tip_force_subscriber_AL_;           ///! Subscriber for topic "/AL_prs"
  
  ros::Publisher desired_joint_state_publisher_;      ///! Publisher for topic "/desired_joint_state"
  ros::Publisher pose_publisher_;                     ///! Publisher for topic "/pose"
  ros::Publisher imu_data_publisher_;                 ///! Publisher for topic "/imu_data"
  ros::Publisher body_velocity_publisher_;            ///! Publisher for topic "/body_velocity"
  ros::Publisher rotation_pose_error_publisher_;      ///! Publisher for topic "/rotation_pose_error"
  
	boost::recursive_mutex mutex_; ///! Mutex used in setup of dynamic reconfigure server
	dynamic_reconfigure::Server<syropod_highlevel_controller::DynamicConfig>* dynamic_reconfigure_server_;

  shared_ptr<Model> model_;                    ///! Pointer to robot model object
  shared_ptr<WalkController> walker_;          ///! Pointer to walk controller object
  shared_ptr<PoseController> poser_;           ///! Pointer to pose controller object
  shared_ptr<ImpedanceController> impedance_;  ///! Pointer to impedance controller object
  DebugOutput debug_;                          ///! Debug class object used for RVIZ visualization
  Parameters params_;                          ///! Parameter data structure for storing parameter variables
	
	bool initialised_ = false; ///! Flags if the state controller has initialised

  SystemState system_state_ = SUSPENDED;     ///! Current state of the entire high-level controller system
	SystemState new_system_state_ = SUSPENDED; ///! Desired state of the entire high_level controller system
	
	RobotState robot_state_ = UNKNOWN;         ///! Current state of the robot
  RobotState new_robot_state_ = UNKNOWN;     ///! Desired state of the robot

  GaitDesignation gait_selection_ = GAIT_UNDESIGNATED;            ///! Current gait selection for the walk cycle
  PosingMode posing_mode_ = NO_POSING;                            ///! Current posing mode for manual posing
  CruiseControlMode cruise_control_mode_ = CRUISE_CONTROL_OFF;    ///! Current cruise control mode
  AutoNavigationMode auto_navigation_mode_ = AUTO_NAVIGATION_OFF; ///! Current auto navigation mode

  ParameterSelection parameter_selection_ = NO_PARAMETER_SELECTION; ///! Currently selected adjustable parameter
  AdjustableParameter* dynamic_parameter_;                          ///! Pointer to the selected parameter object
	double new_parameter_value_ = 0.0;                                ///! New value to assign to the selected parameter

  LegDesignation primary_leg_selection_ = LEG_UNDESIGNATED;   ///! Current primary leg selection designation
  LegDesignation secondary_leg_selection_ = LEG_UNDESIGNATED; ///! Current secondary leg selection designation
  LegState primary_leg_state_ = WALKING;                      ///! State of primary leg selection
  LegState secondary_leg_state_ = WALKING;                    ///! State of secondary leg selection
  shared_ptr<Leg> primary_leg_;                               ///! Pointer to leg object of primary leg selection
  shared_ptr<Leg> secondary_leg_;                             ///! Pointer to leg object of secondary leg selection
  
  int manual_leg_count_ = 0;                  ///! Count of legs that are currently in manual manipulation mode

  bool gait_change_flag_ = false;             ///! Flags that the gait is changing
  bool toggle_primary_leg_state_ = false;     ///! Flags that the primary selected leg state is toggling
  bool toggle_secondary_leg_state_ = false;   ///! Flags that the secondary selected leg state is toggling
  bool parameter_adjust_flag_ = false;        ///! Flags that the selected parameter is being adjusted
  bool apply_new_parameter_ = true;           ///! Flags that the new parameter value is to be applied to the controller
  bool joint_positions_initialised_ = false;  ///! Flags if all joint objects have been initialised with a position
  bool transition_state_flag_ = false;        ///! Flags that the system state is transitioning

  Vector2d linear_velocity_input_;            ///! Input for the desired linear velocity of the robot body
  double angular_velocity_input_ = 0;         ///! Input for the desired angular velocity of the robot body
  Vector3d primary_tip_velocity_input_;       ///! Input for the desired linear velocity of the primary leg tip
  Vector3d secondary_tip_velocity_input_;     ///! Input for the desired linear velocity of the secondary leg tip
  Vector2d linear_cruise_velocity_;           ///! Desired constant linear body velocity for cruise control mode
  double angular_cruise_velocity_;            ///! Desired constant angular body velocity for cruise control mode

  LegContainer::iterator leg_it_;     ///! Leg iteration member variable used to minimise code
  JointContainer::iterator joint_it_; ///! Joint iteration member variable used to minimise code

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

/***********************************************************************************************************************
***********************************************************************************************************************/
#endif /* SYROPOD_HIGHLEVEL_CONTROLLER_STATE_CONTROLLER_H */