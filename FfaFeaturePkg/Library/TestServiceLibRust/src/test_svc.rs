use ec_service_lib::{Result, Service};
use log::{debug, error, info};
use odp_ffa::{MsgSendDirectReq2, MsgSendDirectResp2, Payload, RegisterPayload};
use odp_ffa::{Function, NotificationSet};
use uuid::{uuid, Uuid};
use aarch64_cpu::registers::{CNTFRQ_EL0, CNTVCT_EL0, Readable, Writeable};

// Protocol CMD definitions for Test
#[allow(dead_code)]
const TEST_OPCODE_BASE: u64 = 0xDEF0;
const TEST_OPCODE_TEST_NOTIFICATION: u64 = 0xDEF1;

/* Test Service Defines */
const DELAYED_SRI_BIT_POS: u64 = 1;

#[derive(Default)]
struct GenericRsp {
    status: i64,
}

impl From<GenericRsp> for RegisterPayload {
    fn from(value: GenericRsp) -> Self {
        RegisterPayload::from_iter(value.status.to_le_bytes())
    }
}

#[derive(Default)]
pub struct Test {}

impl Test {
    pub fn new() -> Self {
        Self::default()
    }

    fn notification_handler(&self, msg: &MsgSendDirectReq2) -> GenericRsp {
        // Grab the uuid from the message, they will be at x5 and x6
        let sender_uuid = Uuid::from_u128_le(((msg.register_at(2) as u128) << 64) | (msg.register_at(1) as u128));
        let cookie = msg.register_at(3);
        let flag = 1 << DELAYED_SRI_BIT_POS;
        let bit_pos = 1 << cookie;

        debug!("notification_handler for {:?}", sender_uuid);

        // Prepare for the delay
        info!("init() - reading CNTFRQ_EL0");
        let frequency = CNTFRQ_EL0.get();

        info!(
            "init() - frequency: {}",
            frequency
        );

        // Hot looping here for 20 seconds
        let wait_ticks = 20 * frequency;
        let start = CNTVCT_EL0.get();
        while CNTVCT_EL0.get() - start < wait_ticks {}

        // Set up notification through the Notify service
        // TODO;
        // let notify_msg = MsgSendDirectReq2::new(
        //     Function::Notify,
        //     0)
        NotificationSet::new(msg.source_id(), msg.destination_id(), flag, bit_pos)
            .exec()
            .unwrap();

        GenericRsp {
            status: 0x0,
        }
    }
}

const UUID: Uuid = uuid!("e0fad9b3-7f5c-42c5-b2ee-b7a82313cdb2");

impl Service for Test {
    fn service_name(&self) -> &'static str {
        "Test"
    }

    fn service_uuid(&self) -> Uuid {
        UUID
    }

    async fn ffa_msg_send_direct_req2(&mut self, msg: MsgSendDirectReq2) -> Result<MsgSendDirectResp2> {
        let cmd = msg.u64_at(0);
        debug!("Received Test command 0x{:x}", cmd);

        let payload = match cmd {
            TEST_OPCODE_TEST_NOTIFICATION => RegisterPayload::from(self.notification_handler(&msg)),
            _ => {
                error!("Unknown Test Command: {}", cmd);
                return Err(odp_ffa::Error::Other("Unknown Test Command"));
            }
        };

        Ok(MsgSendDirectResp2::from_req_with_payload(&msg, payload))
    }
}
